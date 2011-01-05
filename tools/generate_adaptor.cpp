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
#include "generate_adaptor.h"

using namespace std;
using namespace DBus;

extern const char *tab;
extern const char *header;
extern const char *dbus_includes;

/*
 * Convert '-' characters found in the XML
 * introspection into '_' characters, so
 * that the result will be a legal C++
 * identifier.
 */
static string legalize(string input)
{
	size_t pos = 0;
  
	while (pos != string::npos) {
		pos = input.find('-', pos);
		if (pos != string::npos)
			input[pos] = '_';
	}
	return input;
}

/*! Generate adaptor code for object methods
  */
void generate_methods(const Xml::Nodes &methods, ostringstream &body) {

	for (Xml::Nodes::const_iterator mi = methods.begin(); mi != methods.end(); ++mi)
	{
		Xml::Node &method = **mi;
		Xml::Nodes args = method["arg"];
		Xml::Nodes args_in = args.select("direction","in");
		Xml::Nodes args_out = args.select("direction","out");

		body << tab << "virtual ";

		if (args_out.size() == 0 || args_out.size() > 1)
		{
			body << "void ";
		}
		else if (args_out.size() == 1)
		{
			body << signature_to_type(args_out.front()->get("type")) << " ";
		}

		// generate the method name
		body << method.get("name") << "(";

		// generate the methods 'in' variables
		unsigned int i = 0;
		for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
		{
			Xml::Node &arg = **ai;
			body << "const " << signature_to_type(arg.get("type")) << "& ";

			string arg_name = legalize(arg.get("name"));
			if (arg_name.length())
				body << arg_name;

			body << ", ";
		}

		// generate the method 'out' variables if multiple 'out' values exist
		if (args_out.size() > 1)
		{
			unsigned int i = 0;
			for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
			{
				Xml::Node &arg = **ao;
				body << signature_to_type(arg.get("type")) << "&";

				string arg_name = legalize(arg.get("name"));
				if (arg_name.length())
					body << " " << arg_name;

				body << ", ";
			}
		}

		body << "::DBus::Error &error";
		body << ") = 0;" << endl;
	}
}

/*! Generate adaptor code for a XML introspection
  */
void generate_adaptor(Xml::Document &doc, const char *filename)
{
	ostringstream body;
	ostringstream head;
	vector <string> include_vector;

	head << header;
	string filestring = filename;
	underscorize(filestring);

	string cond_comp = "__dbusxx__" + filestring + "__ADAPTOR_MARSHAL_H";

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

		// gets the name of a interface: <interface name="XYZ">
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

		// a "_adaptor" is added to class name to distinguish between proxy and adaptor
		ifaceclass += "_adaptor";

		cerr << "generating code for interface " << ifacename << "..." << endl;

		// the code from class definiton up to opening of the constructor is generated...
		body << "class " << ifaceclass << endl
		<< ": public ::DBus::InterfaceAdaptor" << endl
		<< "{" << endl
		<< "public:" << endl
		<< endl
		<< tab << ifaceclass << "()" << endl
		<< tab << ": ::DBus::InterfaceAdaptor(\"" << ifacename << "\")" << endl
		<< tab << "{" << endl;

		// generates code to bind the properties
		for (Xml::Nodes::iterator pi = properties.begin(); pi != properties.end(); ++pi)
		{
			Xml::Node &property = **pi;

			body << tab << tab << "bind_property("
			<< legalize(property.get("name")) << ", "
			<< "\"" << property.get("type") << "\", "
			<< (property.get("access").find("read") != string::npos
				? "true"
				: "false")
			<< ", "
			<< (property.get("access").find("write") != string::npos
				? "true"
				: "false")
			<< ");" << endl;
		}

		// generate code to register all methods
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;

			body << tab << tab << "register_method("
			<< ifaceclass << ", " << method.get("name") << ", "<< stub_name(method.get("name"))
			<< ");" << endl;
		}

		body << tab << "}" << endl
		<< endl;

		body << tab << "::DBus::IntrospectedInterface *const introspect() const " << endl
		<< tab << "{" << endl;

		// generate the introspect arguments
		for (Xml::Nodes::iterator mi = ms.begin(); mi != ms.end(); ++mi)
		{
			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];

			body << tab << tab << "static ::DBus::IntrospectedArgument " << method.get("name") << "_args[] =" << endl
			<< tab << tab << "{" << endl;

			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai)
			{
				Xml::Node &arg = **ai;

				body << tab << tab << tab << "{ ";

				if (arg.get("name").length())
				{
					body << "\"" << legalize(arg.get("name")) << "\", ";
				}
				else
				{
					body << "0, ";
				}
				body << "\"" << arg.get("type") << "\", "
				<< (arg.get("direction") == "in" ? "true" : "false")
				<< " }," << endl;
			}
			body << tab << tab << tab << "{ 0, 0, 0 }" << endl
			<< tab << tab << "};" << endl;
		}

		body << tab << tab << "static ::DBus::IntrospectedMethod " << ifaceclass << "_methods[] =" << endl
		<< tab << tab << "{" << endl;

		// generate the introspect methods
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;

			body << tab << tab << tab << "{ \"" << method.get("name") << "\", " << method.get("name") << "_args }," << endl;
		}

		body << tab << tab << tab << "{ 0, 0 }" << endl
		<< tab << tab << "};" << endl;

		body << tab << tab << "static ::DBus::IntrospectedMethod " << ifaceclass << "_signals[] =" << endl
		<< tab << tab << "{" << endl;

		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &method = **si;

			body << tab << tab << tab << "{ \"" << method.get("name") << "\", " << method.get("name") << "_args }," << endl;
		}

		body << tab << tab << tab << "{ 0, 0 }" << endl
		<< tab << tab << "};" << endl;

		body << tab << tab << "static ::DBus::IntrospectedProperty " << ifaceclass << "_properties[] =" << endl
		<< tab << tab << "{" << endl;

		for (Xml::Nodes::iterator pi = properties.begin(); pi != properties.end(); ++pi)
		{
			Xml::Node &property = **pi;

			body << tab << tab << tab << "{ "
			<< "\"" << legalize(property.get("name")) << "\", "
			<< "\"" << property.get("type") << "\", "
			<< (property.get("access").find("read") != string::npos
				? "true"
				: "false")
			<< ", "
			<< (property.get("access").find("write") != string::npos
				? "true"
				: "false")
			<< " }," << endl;
		}


		body << tab << tab << tab << "{ 0, 0, 0, 0 }" << endl
		<< tab << tab << "};" << endl;

		// generate the Introspected interface
		body << tab << tab << "static ::DBus::IntrospectedInterface " << ifaceclass << "_interface =" << endl
		<< tab << tab << "{" << endl
		<< tab << tab << tab << "\"" << ifacename << "\"," << endl
		<< tab << tab << tab << ifaceclass << "_methods," << endl
		<< tab << tab << tab << ifaceclass << "_signals," << endl
		<< tab << tab << tab << ifaceclass << "_properties" << endl
		<< tab << tab << "};" << endl
		<< tab << tab << "return &" << ifaceclass << "_interface;" << endl
		<< tab << "}" << endl
		<< endl;

		body << "public:" << endl
		<< endl
		<< tab << "/* properties exposed by this interface, use" << endl
		<< tab << " * property() and property(value) to get and set a particular property" << endl
		<< tab << " */" << endl;

		// generate the properties code
		for (Xml::Nodes::iterator pi = properties.begin(); pi != properties.end(); ++pi)
		{
			Xml::Node &property = **pi;
			string name = legalize(property.get("name"));
			string type = property.get("type");
			string type_name = signature_to_type(type);

			body << tab << "::DBus::PropertyAdaptor< " << type_name << " > " << name << ";" << endl;
		}

		body << endl;

		body << "public:" << endl
		<< endl
		<< tab << "/* methods exported by this interface," << endl
		<< tab << " * you will have to implement them in your ObjectAdaptor" << endl
		<< tab << " */" << endl;

		// generate the methods code
		generate_methods(methods, body);

		body << endl
		<< "public:" << endl
		<< endl
		<< tab << "/* signal emitters for this interface" << endl
		<< tab << " */" << endl;

		// generate the signals code
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			body << tab << "void " << legalize(signal.get("name")) << "(";

			// generate the signal arguments
			unsigned int i = 0;
			for (Xml::Nodes::iterator a = args.begin(); a != args.end(); ++a, ++i)
			{
				Xml::Node &arg = **a;

				body << "const " << signature_to_type(arg.get("type")) << "& arg" << i+1;

				if (i+1 != args.size())
					body << ", ";
			}

			body << ")" << endl
			<< tab << "{" << endl
			<< tab << tab << "::DBus::SignalMessage sig(\"" << legalize(signal.get("name")) <<"\");" << endl;;

			// generate the signal body
			if (args.size() > 0)
			{
				body << tab << tab << "::DBus::MessageIter wi = sig.writer();" << endl;

				for (unsigned int i = 0; i < args.size(); ++i)
				{
					body << tab << tab << "wi << arg" << i+1 << ";" << endl;
				}
			}

			// emit the signal in method body
			body << tab << tab << "emit_signal(sig);" << endl
			<< tab << "}" << endl;
		}

		body << endl
		<< "private:" << endl
		<< endl
		<< tab << "/* unmarshalers (to unpack the DBus message before calling the actual interface method)" << endl
		<< tab << " */" << endl;

		// generate the unmarshalers
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];
			Xml::Nodes args_in = args.select("direction","in");
			Xml::Nodes args_out = args.select("direction","out");

			body << tab << "::DBus::Message " << stub_name(method.get("name"))
				 << "(const ::DBus::CallMessage &call)" << endl
			<< tab << "{" << endl
			<< tab << tab << "::DBus::MessageIter ri = call.reader();" << endl
			<< endl;

			body << tab << tab << "::DBus::Error error;" << endl;
			// generate the 'in' variables
			unsigned int i = 1;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				body << tab << tab << signature_to_type(arg.get("type")) << " argin" << i << ";"
				<< " ri >> argin" << i << ";" << endl;
			}

			// generate 'out' object variables
			if (args_out.size() == 0)
			{
				body << tab << tab;
			}
			else if (args_out.size() == 1)
			{
				body << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout1 = ";
			}
			else
			{
				unsigned int i = 1;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					body << tab << tab << signature_to_type(arg.get("type")) << " argout" << i << ";" << endl;
				}
				body << tab << tab;
			}

			body << method.get("name") << "(";

			for (unsigned int i = 0; i < args_in.size(); ++i)
			{
				body << "argin" << i+1;
				body << ", ";
			}

			if (args_out.size() > 1)
				for (unsigned int i = 0; i < args_out.size(); ++i)
				{
					body << "argout" << i+1;
					body << ", ";
				}

			body << "error";
			body << ");" << endl;

			body << tab << tab << "if (error.is_set())" << endl
				 << tab << tab << "{" << endl
				 << tab << tab << tab
				 << "return ::DBus::ErrorMessage(call, error.name(), error.message());"
				 << endl
				 << tab << tab << "}" << endl;


			body << tab << tab << "::DBus::ReturnMessage reply(call);" << endl;

			if (args_out.size() > 0)
			{
				body << tab << tab << "::DBus::MessageIter wi = reply.writer();" << endl;

				for (unsigned int i = 0; i < args_out.size(); ++i)
				{
					body << tab << tab << "wi << argout" << i+1 << ";" << endl;
				}
			}

			body << tab << tab << "return reply;" << endl;

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
