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

/*! Generate proxy code for a XML introspection
  */
void generate_proxy(Xml::Document &doc, const char *filename)
{
	cerr << "writing " << filename << endl;

	ofstream file(filename);
	if (file.bad())
	{
		cerr << "unable to write file " << filename << endl;
		exit(-1);
	}

	file << header;
	string filestring = filename;
	underscorize(filestring);

	string cond_comp = "__dbusxx__" + filestring + "__PROXY_MARSHAL_H";

	file << "#ifndef " << cond_comp << endl;
	file << "#define " << cond_comp << endl;

	file << dbus_includes;

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
		if (ifacename == "org.freedesktop.DBus.Introspectable"
		 ||ifacename == "org.freedesktop.DBus.Properties")
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

			file << "namespace " << nspace << " {" << endl;

			++nspaces;
		}
		file << endl;

		string ifaceclass;

		getline(ss, ifaceclass);

		// a "_proxy" is added to class name to distinguish between proxy and adaptor
		ifaceclass += "_proxy";

		cerr << "generating code for interface " << ifacename << "..." << endl;

		// the code from class definiton up to opening of the constructor is generated...
		file << "class " << ifaceclass << endl
		     << " : public ::DBus::InterfaceProxy" << endl
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

			file << tab << tab << "connect_signal(" 
			     << ifaceclass << ", " << signal.get("name") << ", " << stub_name(signal.get("name"))
			     << ");" << endl;
		}

		// the constructor ends here
		file << tab << "}" << endl
		     << endl;

		// write public block header for properties
		file << "public:" << endl << endl
		     << tab << "/* properties exported by this interface */" << endl;

		// this loop generates all properties
		for (Xml::Nodes::iterator pi = properties.begin ();
		     pi != properties.end (); ++pi)
		{
			Xml::Node & property = **pi;
			string prop_name = property.get ("name");
			string property_access = property.get ("access");
			if (property_access == "read" || property_access == "readwrite")
			{
				file << tab << tab << "const " << signature_to_type (property.get("type"))
				     << " " << prop_name << "() {" << endl;
				file << tab << tab << tab << "::DBus::CallMessage call ;\n ";
				file << tab << tab << tab
				     << "call.member(\"Get\"); call.interface(\"org.freedesktop.DBus.Properties\");"
				     << endl;
				file << tab << tab << tab
				     << "::DBus::MessageIter wi = call.writer(); " << endl;
				file << tab << tab << tab
				     << "const std::string interface_name = \"" << ifacename << "\";"
				     << endl;
				file << tab << tab << tab
				     << "const std::string property_name  = \"" << prop_name << "\";"
				     << endl;
				file << tab << tab << tab << "wi << interface_name;" << endl;
				file << tab << tab << tab << "wi << property_name;" << endl;
				file << tab << tab << tab
				     << "::DBus::Message ret = this->invoke_method (call);" << endl;
				file << tab << tab << tab
				     << "::DBus::MessageIter ri = ret.reader ();" << endl;
				file << tab << tab << tab << "::DBus::Variant argout; " << endl;
				file << tab << tab << tab << "ri >> argout;" << endl;
				file << tab << tab << tab << "return argout;" << endl;
				file << tab << tab << "};" << endl;
			}

			if (property_access == "write" || property_access == "readwrite")
			{
				file << tab << tab << "void " << prop_name << "( const "<< signature_to_type (property.get("type")) << " & input" << ") {" << endl;
				file << tab << tab << tab << "::DBus::CallMessage call ;\n ";
				file << tab << tab << tab <<"call.member(\"Set\");  call.interface( \"org.freedesktop.DBus.Properties\");"<< endl;
				file << tab << tab << tab <<"::DBus::MessageIter wi = call.writer(); " << endl;
				file << tab << tab << tab <<"::DBus::Variant value;" << endl;
				file << tab << tab << tab <<"::DBus::MessageIter vi = value.writer ();" << endl;
				file << tab << tab << tab <<"vi << input;" << endl;
				file << tab << tab << tab <<"const std::string interface_name = \"" << ifacename << "\";" << endl;
				file << tab << tab << tab <<"const std::string property_name  = \"" << prop_name << "\";"<< endl;
				file << tab << tab << tab <<"wi << interface_name;" << endl;
				file << tab << tab << tab <<"wi << property_name;" << endl;
				file << tab << tab << tab <<"wi << value;" << endl;
				file << tab << tab << tab <<"::DBus::Message ret = this->invoke_method (call);" << endl;
				file << tab << tab << "};" << endl;
            }
		}

		// write public block header for methods
		file << "public:" << endl
		     << endl
		     << tab << "/* methods exported by this interface," << endl
		     << tab << " * this functions will invoke the corresponding methods on the remote objects" << endl
		     << tab << " */" << endl;

		// this loop generates all methods
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];
			Xml::Nodes args_in = args.select("direction","in");
			Xml::Nodes args_out = args.select("direction","out");

			if (args_out.size() == 0 || args_out.size() > 1)
			{
				file << tab << "void ";
			}
			else if (args_out.size() == 1)
			{
				file << tab << signature_to_type(args_out.front()->get("type")) << " ";
			}

			file << method.get("name") << "(";
			
			// generate all 'in' arguments for a method signature
			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				file << "const " << signature_to_type(arg.get("type")) << "& ";

				string arg_name = arg.get("name");
				if (arg_name.length())
					file << arg_name;
				else
					file << "argin" << i;

				if ((i+1 != args_in.size() || args_out.size() > 1))
					file << ", ";
			}

			if (args_out.size() > 1)
			{
				unsigned int i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					file << signature_to_type(arg.get("type")) << "&";

					string arg_name = arg.get("name");
					if (arg_name.length())
						file << " " << arg_name;
					else
						file << " argout" << i;

					if (i+1 != args_out.size())
						file << ", ";
				}
			}
			file << ")" << endl;

			file << tab << "{" << endl
			     << tab << tab << "::DBus::CallMessage call;" << endl;

			if (args_in.size() > 0)
			{
				file << tab << tab << "::DBus::MessageIter wi = call.writer();" << endl
				     << endl;
			}

			unsigned int j = 0;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++j)
			{
				Xml::Node &arg = **ai;
				string arg_name = arg.get("name");
				if (arg_name.length())
					file << tab << tab << "wi << " << arg_name << ";" << endl;
				else
					file << tab << tab << "wi << argin" << j << ";" << endl;
			}

			file << tab << tab << "call.member(\"" << method.get("name") << "\");" << endl
			     << tab << tab << "::DBus::Message ret = invoke_method(call);" << endl;


			if (args_out.size() > 0)
			{
				file << tab << tab << "::DBus::MessageIter ri = ret.reader();" << endl
				     << endl;
			}

			if (args_out.size() == 1)
			{
				file << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout;" << endl;
				file << tab << tab << "ri >> argout;" << endl;
				file << tab << tab << "return argout;" << endl;
			}
			else if (args_out.size() > 1)
			{
				unsigned int i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;

					string arg_name = arg.get("name");
					if (arg_name.length())
						file << tab << tab << "ri >> " << arg.get("name") << ";" << endl;
					else
						file << tab << tab << "ri >> argout" << i << ";" << endl;
				}
			}

			file << tab << "}" << endl
			     << endl;
		}

		// write public block header for signals
		file << endl
		     << "public:" << endl
		     << endl
		     << tab << "/* signal handlers for this interface" << endl
		     << tab << " */" << endl;

		// this loop generates all signals
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			file << tab << "virtual void " << signal.get("name") << "(";

			// this loop generates all argument for a signal
			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				file << "const " << signature_to_type(arg.get("type")) << "& ";

				string arg_name = arg.get("name");
				if (arg_name.length())
					file << arg_name;
				else
					file << "argin" << i;

				if ((ai+1 != args.end()))
					file << ", ";
			}
			file << ") = 0;" << endl;
		}

		// write private block header for unmarshalers
		file << endl
		     << "private:" << endl
		     << endl
		     << tab << "/* unmarshalers (to unpack the DBus message before calling the actual signal handler)" << endl
		     << tab << " */" << endl;

		// generate all the unmarshalers
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			file << tab << "void " << stub_name(signal.get("name")) << "(const ::DBus::SignalMessage &sig)" << endl
			     << tab << "{" << endl;

			if (args.size() > 0)
			{
				file << tab << tab << "::DBus::MessageIter ri = sig.reader();" << endl
				     << endl;
			}

			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				file << tab << tab << signature_to_type(arg.get("type")) << " " ;

				string arg_name = arg.get("name");
				if (arg_name.length())
					file << arg_name << ";" << " ri >> " << arg_name << ";" << endl;
				else
					file << "arg" << i << ";" << " ri >> " << "arg" << i << ";" << endl;
			}

			file << tab << tab << signal.get("name") << "(";

			unsigned int j = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++j)
			{
				Xml::Node &arg = **ai;

				string arg_name = arg.get("name");
				if (arg_name.length())
					file << arg_name;
				else
					file << "arg" << j;

				if (ai+1 != args.end())
					file << ", ";
			}

			file << ");" << endl;

			file << tab << "}" << endl;
		}


		file << "};" << endl
		     << endl;

		for (unsigned int i = 0; i < nspaces; ++i)
		{
			file << "} ";
		}
		file << endl;
	}

	file << "#endif//" << cond_comp << endl;

	file.close();
}
