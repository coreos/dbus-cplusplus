/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "generator_utils.h"
#include "generate_proxy.h"

using namespace std;
using namespace DBus;

extern const char *tab;
extern const char *header;
extern const char *dbus_includes;

#define WRITE_VISIBILITY(v) \
		if (visibility != v) {\
			visibility = v; \
			body << visibility << ":" << endl << endl; \
		}

/*! Generate proxy code for a XML introspection
  */
void generate_methods(ostringstream &body, string &ifaceclass, Xml::Nodes &methods, bool async_mode) {
    const char *mode = async_mode ? "non-blocking versions of the " : "";
    // write public block header for methods
    body << tab << "/* " << mode << "methods exported by this interface." << endl
	 << tab << " * these functions will invoke the corresponding methods on the remote objects" << endl
	 << tab << " */" << endl;

    // this loop generates all methods
    for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
    {
	Xml::Node &method = **mi;
	Xml::Nodes args = method["arg"];
	Xml::Nodes args_in = args.select("direction", "in");
	Xml::Nodes args_out = args.select("direction", "out");
	string name(method.get("name"));

	if (async_mode || args_out.size() == 0 || args_out.size() > 1)
	{
	    body << tab << "void ";
	}
	else if (args_out.size() == 1)
	{
	    body << tab << signature_to_type(args_out.front()->get("type")) << " ";
	}

	body << name << "(";

	// generate all 'in' arguments for a method signature
	unsigned int i = 0;
	for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
	{
	    Xml::Node &arg = **ai;
	    body << "const " << signature_to_type(arg.get("type")) << "& ";

	    string arg_name = arg.get("name");
	    if (arg_name.length())
		body << legalize(arg_name);
	    else
		body << "argin" << i;

	    if (async_mode || i+1 != args_in.size() || args_out.size() > 1)
		body << ", ";
	}
	if (async_mode)
		body << "void* __data, int __timeout=-1";

	if (!async_mode && args_out.size() > 1)
	{
	    unsigned int i = 0;
	    for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
	    {
		Xml::Node &arg = **ao;
		body << signature_to_type(arg.get("type")) << "&";

		string arg_name = arg.get("name");
		if (arg_name.length())
		    body << " " << legalize(arg_name);
		else
		    body << " argout" << i;

		if (i+1 != args_out.size())
		    body << ", ";
	    }
	}
	body << ")" << endl;

	body << tab << "{" << endl
	     << tab << tab << "::DBus::CallMessage __call;" << endl;

	if (args_in.size() > 0)
	{
	    body << tab << tab << "::DBus::MessageIter __wi = __call.writer();" << endl
		 << endl;
	}

	unsigned int j = 0;
	for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++j)
	{
	    Xml::Node &arg = **ai;
	    string arg_name = arg.get("name");
	    if (arg_name.length())
		body << tab << tab << "__wi << " << legalize(arg_name) << ";" << endl;
	    else
		body << tab << tab << "__wi << argin" << j << ";" << endl;
	}

	body << tab << tab << "__call.member(\"" << name << "\");" << endl;
	if (async_mode)
	{
	    body << tab << tab << "::DBus::PendingCall *__pending = invoke_method_async(__call, __timeout);" << endl;
	    body << tab << tab << "::DBus::AsyncReplyHandler __handler;" << endl;
	    body << tab << tab << "__handler = new ::DBus::Callback<" << ifaceclass
		 << ", void, ::DBus::PendingCall *>(this, &" << ifaceclass << "::_"
		 << name << "Callback_stub);" << endl;
	    body << tab << tab << "__pending->reply_handler(__handler);" << endl;
	    body << tab << tab << "__pending->data(__data);" << endl;
	} else {
	    if (args_out.size() > 0)
	    {
		body << tab << tab << "::DBus::Message __ret = invoke_method(__call);" << endl;
		body << tab << tab << "::DBus::MessageIter __ri = __ret.reader();" << endl
		     << endl;
	    } else
	    {
		body << tab << tab << "invoke_method(__call);" << endl;
	    }

	    if (args_out.size() == 1)
	    {
		body << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout;" << endl;
		body << tab << tab << "__ri >> argout;" << endl;
		body << tab << tab << "return argout;" << endl;
	    }
	    else if (args_out.size() > 1)
	    {
		unsigned int i = 0;
		for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
		{
		    Xml::Node &arg = **ao;

		    string arg_name = arg.get("name");
		    if (arg_name.length())
			body << tab << tab << "__ri >> " << legalize(arg.get("name")) << ";" << endl;
		    else
			body << tab << tab << "__ri >> argout" << i << ";" << endl;
		}
	    }
	}

	body << tab << "}" << endl << endl;
    }
}

void generate_proxy(Xml::Document &doc, const char *filename, bool sync_mode, bool async_mode)
{
	ostringstream body;
	ostringstream head;
	vector <string> include_vector;
	string visibility;

	head << header;
	string filestring = filename;
	underscorize(filestring);

	string cond_comp = "__dbusxx__" + filestring + "__PROXY_MARSHAL_H";

	head << "#ifndef " << cond_comp << endl
		 << "#define " << cond_comp << endl;

	head << dbus_includes;

	Xml::Node &root = *(doc.root);
	Xml::Nodes interfaces = root["interface"];

	// iterate over all interface definitions
	for (Xml::Nodes::iterator i = interfaces.begin(); i != interfaces.end(); ++i)
	{
		Xml::Node &iface = **i;
		Xml::Nodes methods = iface["method"];
		Xml::Nodes signals = iface["signal"];
		Xml::Nodes properties = iface["property"];
		Xml::Nodes ms;
		ms.insert(ms.end(), methods.begin(), methods.end());
		ms.insert(ms.end(), signals.begin(), signals.end());

		// gets the name of an interface: <interface name="XYZ">
		string ifacename = iface.get("name");

		// these interface names are skipped.
		if (ifacename == "org.freedesktop.DBus.Introspectable")
		{
			cerr << "skipping interface " << ifacename << endl;
			continue;
		}

		istringstream ss(ifacename);
		string nspace;
		unsigned int nspaces = 0;

		// prints all the namespaces defined with <interface name="X.Y.Z">
		while (ss.str().find('.', ss.tellg()) != string::npos)
		{
			getline(ss, nspace, '.');

			body << "namespace " << nspace << " {" << endl;

			++nspaces;
		}
		body << endl;

		string ifaceclass;

		getline(ss, ifaceclass);

		// a "_proxy" is added to class name to distinguish between proxy and adaptor
		ifaceclass += "_proxy";

		cerr << "generating code for interface " << ifacename << "..." << endl;

		// the code from class definiton up to opening of the constructor is generated...
		body << "class " << ifaceclass << endl
		<< ": public ::DBus::InterfaceProxy" << endl
		<< "{" << endl
		<< "public:" << endl
		<< endl
		<< tab << ifaceclass << "()" << endl
		<< tab << ": ::DBus::InterfaceProxy(\"" << ifacename << "\")" << endl
		<< tab << "{" << endl;

		// generates code to connect all the signal stubs; this is still inside the constructor
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;

			string marshname = "_" + signal.get("name") + "_stub";

			body << tab << tab << "connect_signal("
			<< ifaceclass << ", " << signal.get("name") << ", " << stub_name(signal.get("name"))
			<< ");" << endl;
		}

		// the constructor ends here
		body << tab << "}" << endl
		<< endl;

		// write public block header for properties
		WRITE_VISIBILITY("public");
		body << tab << "/* properties exported by this interface */" << endl << endl;

		// this loop generates all properties
		for (Xml::Nodes::iterator pi = properties.begin ();
		        pi != properties.end (); ++pi)
		{
			Xml::Node & property = **pi;
			string prop_name = property.get ("name");
			string property_access = property.get ("access");
			if (property_access == "read" || property_access == "readwrite")
			{
				body << tab;
				if (!is_primitive_type(property.get("type"))) {
					body << "const ";
				}
				body << signature_to_type(property.get("type"))
				<< " " << legalize(prop_name) << "() {" << endl;
				body << tab << tab << "::DBus::CallMessage __call ;\n ";
				body << tab << tab
				<< "__call.member(\"Get\"); __call.interface(\"org.freedesktop.DBus.Properties\");"
				<< endl;
				body << tab << tab
				<< "::DBus::MessageIter __wi = __call.writer(); " << endl;
				body << tab << tab
				<< "const std::string interface_name = \"" << ifacename << "\";"
				<< endl;
				body << tab << tab
				<< "const std::string property_name  = \"" << prop_name << "\";"
				<< endl;
				body << tab << tab << "__wi << interface_name;" << endl;
				body << tab << tab << "__wi << property_name;" << endl;
				body << tab << tab
				<< "::DBus::Message __ret = this->invoke_method (__call);" << endl;
				body << tab << tab
				<< "::DBus::MessageIter __ri = __ret.reader ();" << endl;
				body << tab << tab << "::DBus::Variant argout; " << endl;
				body << tab << tab << "__ri >> argout;" << endl;
				body << tab << tab << "return argout;" << endl;
				body << tab << "}" << endl;
			}

			if (property_access == "write" || property_access == "readwrite")
			{
				body << tab << "void " << legalize(prop_name) << "( const "<< signature_to_type (property.get("type")) << " & input" << ") {" << endl;
				body << tab << tab << "::DBus::CallMessage __call ;\n ";
				body << tab << tab <<"__call.member(\"Set\");  __call.interface( \"org.freedesktop.DBus.Properties\");"<< endl;
				body << tab << tab <<"::DBus::MessageIter __wi = __call.writer(); " << endl;
				body << tab << tab <<"::DBus::Variant __value;" << endl;
				body << tab << tab <<"::DBus::MessageIter vi = __value.writer ();" << endl;
				body << tab << tab <<"vi << input;" << endl;
				body << tab << tab <<"const std::string interface_name = \"" << ifacename << "\";" << endl;
				body << tab << tab <<"const std::string property_name  = \"" << prop_name << "\";"<< endl;
				body << tab << tab <<"__wi << interface_name;" << endl;
				body << tab << tab <<"__wi << property_name;" << endl;
				body << tab << tab <<"__wi << __value;" << endl;
				body << tab << tab <<"::DBus::Message __ret = this->invoke_method (__call);" << endl;
				body << tab << "}" << endl;
			}
			body << endl;
		}

		if (sync_mode)
		{
			WRITE_VISIBILITY("public");
			generate_methods(body, ifaceclass, methods, false);
		}
		if (async_mode)
		{
			WRITE_VISIBILITY("public");
			generate_methods(body, ifaceclass, methods, true);
		}


		// write public block header for signals
		WRITE_VISIBILITY("public");
		body << tab << "/* signal handlers for this interface." << endl
		     << tab << " * you will have to implement them in your ObjectProxy" << endl
		     << tab << " */" << endl;

		// this loop generates all signals
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			body << tab << "virtual void " << signal.get("name") << "(";

			// this loop generates all argument for a signal
			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				body << "const " << signature_to_type(arg.get("type")) << "& ";

				string arg_name = arg.get("name");
				if (arg_name.length())
					body << legalize(arg_name);
				else
					body << "argin" << i;

				if ((ai+1 != args.end()))
					body << ", ";
			}
			body << ") = 0;" << endl;
		}

		if (async_mode)
		{
			// write protected block async method reply handlers
			WRITE_VISIBILITY("protected");
			body << tab << "/* async method reply handlers for this interface." << endl
			<< tab << " * you will have to implement them in your ObjectProxy" << endl
			<< tab << " */ " << endl << endl;
			for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
			{
				Xml::Node &method = **mi;
				Xml::Nodes args = method["arg"];
				Xml::Nodes args_out = args.select("direction", "out");
				string name(method.get("name"));

				body << tab << "virtual void " << name << "Callback(";
				unsigned int i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					body << "const " << signature_to_type(arg.get("type")) << "&";

					string arg_name = arg.get("name");
					if (arg_name.length())
						body << " /*" << legalize(arg_name);
					else
						body << " /*result" << i;
					body << "*/, ";
				}
				body << "const ::DBus::Error&, void*)" << endl;
				body << tab << "{" << endl
				     << tab << tab << "assert(!\"Implement " << name << "Callback\");" << endl
				     << tab << "}" << endl << endl;
			}
			// write private block unmarshallers for async method reply handlers
			body << endl;
			WRITE_VISIBILITY("private");
			body << tab << "/* unmarshallers (to steal the PendingCall reply and unmarshall args before invoking the reply callback)" << endl
			<< tab << " */" << endl;
			for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
			{
				Xml::Node &method = **mi;
				Xml::Nodes args = method["arg"];
				Xml::Nodes args_out = args.select("direction", "out");
				string name(method.get("name"));

				body << tab << "void _" << name << "Callback_stub(::DBus::PendingCall *__call)" << endl
	 			<< tab << "{" << endl
				<< tab << tab << "::DBus::Message __reply = __call->steal_reply();" << endl
				<< tab << tab << "void *__data = __call->data();" << endl
				<< tab << tab << "remove_pending_call(__call);" << endl;
				if (args_out.size() != 0)
					body << tab << tab << "::DBus::MessageIter __ri = __reply.reader();" << endl;
				body << tab << tab << "::DBus::Error __error(__reply);" << endl;
				unsigned int i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;

					string arg_name = arg.get("name");
					body << tab << tab << signature_to_type(arg.get("type")) << " ";
					if (arg_name.length())
						body << legalize(arg_name);
					else
						body << "result" << i;
					body << ";" << endl;
				}
				if (args_out.size() != 0)
					body << tab << tab << "if (!__error.is_set()) {" << endl;
				i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;

					string arg_name = arg.get("name");
					body << tab << tab << tab << "__ri >> ";
					if (arg_name.length())
						body << legalize(arg_name);
					else
						body << "result" << i;
					body << ";" << endl;
				}
				if (args_out.size() != 0)
					body << tab << tab << "}" << endl;
				body << tab << tab << name << "Callback(";
				i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					string arg_name = arg.get("name");
					if (arg_name.length())
						body << legalize(arg_name);
					else
						body << "result" << i;
					body << ", ";
				}
				body << "__error, __data);" << endl;
	 			body << tab << "}" << endl << endl;
			}
		}

		// write private block header for unmarshallers
		body << endl;

		WRITE_VISIBILITY("private");
		body << tab << "/* unmarshallers (to unpack the DBus message before calling the actual signal handler)" << endl
		<< tab << " */" << endl;

		// generate all the unmarshallers
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			body << tab << "void " << stub_name(signal.get("name")) << "(const ::DBus::SignalMessage &sig)" << endl
			<< tab << "{" << endl;

			if (args.size() > 0)
			{
				body << tab << tab << "::DBus::MessageIter __ri = sig.reader();" << endl
				<< endl;
			}

			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				body << tab << tab << signature_to_type(arg.get("type")) << " " ;

				string arg_name = arg.get("name");
				if (arg_name.length())
					body << legalize(arg_name) << ";" << " __ri >> " << legalize(arg_name) << ";" << endl;
				else
					body << "arg" << i << ";" << " __ri >> " << "arg" << i << ";" << endl;
			}

			body << tab << tab << signal.get("name") << "(";

			// generate all arguments for the call to the virtual function
			unsigned int j = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++j)
			{
				Xml::Node &arg = **ai;

				string arg_name = arg.get("name");
				if (arg_name.length())
					body << legalize(arg_name);
				else
					body << "arg" << j;

				if (ai+1 != args.end())
					body << ", ";
			}

			body << ");" << endl;

			body << tab << "}" << endl;
		}

		body << "};" << endl
		<< endl;

		for (unsigned int i = 0; i < nspaces; ++i)
		{
			body << "} ";
		}
		body << endl;
	}

	body << "#endif //" << cond_comp << endl;

	ofstream file(filename);
	if (file.bad())
	{
		cerr << "unable to write file " << filename << endl;
		exit(-1);
	}

	file << head.str ();
	file << body.str ();

	file.close();
}
