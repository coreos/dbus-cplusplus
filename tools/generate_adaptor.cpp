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

/*! Generate adaptor code for a XML introspection
  */
void generate_adaptor(Xml::Document &doc, const char *filename)
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

	string cond_comp = "__dbusxx__" + filestring + "__ADAPTOR_MARSHAL_H";

	file << "#ifndef " << cond_comp << endl
	     << "#define " << cond_comp << endl;

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

        // a "_adaptor" is added to class name to distinguish between proxy and adaptor
		ifaceclass += "_adaptor";

		cerr << "generating code for interface " << ifacename << "..." << endl;

        // the code from class definiton up to opening of the constructor is generated...
		file << "class " << ifaceclass << endl
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

			file << tab << tab << "bind_property("
			     << property.get("name") << ", "
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

			file << tab << tab << "register_method(" 
			     << ifaceclass << ", " << method.get("name") << ", "<< stub_name(method.get("name")) 
			     << ");" << endl;
		}

		file << tab << "}" << endl
		     << endl;

		file << tab << "::DBus::IntrospectedInterface *const introspect() const " << endl
		     << tab << "{" << endl;

		// generate the introspect arguments
		for (Xml::Nodes::iterator mi = ms.begin(); mi != ms.end(); ++mi)
		{
			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];

			file << tab << tab << "static ::DBus::IntrospectedArgument " << method.get("name") << "_args[] = " << endl
			     << tab << tab << "{" << endl;

			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai)
			{
				Xml::Node &arg = **ai;

				file << tab << tab << tab << "{ ";

				if (arg.get("name").length())
				{
					file << "\"" << arg.get("name") << "\", ";
				}
				else
				{
					file << "0, ";
				}
				file << "\"" << arg.get("type") << "\", "
				     << (arg.get("direction") == "in" ? "true" : "false")
				     << " }," << endl;
			}
			file << tab << tab << tab << "{ 0, 0, 0 }" << endl
			     << tab << tab << "};" << endl;
		}

		file << tab << tab << "static ::DBus::IntrospectedMethod " << ifaceclass << "_methods[] = " << endl
		     << tab << tab << "{" << endl;

		// generate the introspect methods
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;
			
			file << tab << tab << tab << "{ \"" << method.get("name") << "\", " << method.get("name") << "_args }," << endl;
		}

		file << tab << tab << tab << "{ 0, 0 }" << endl
		     << tab << tab << "};" << endl;

		file << tab << tab << "static ::DBus::IntrospectedMethod " << ifaceclass << "_signals[] = " << endl
		     << tab << tab << "{" << endl;

		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &method = **si;
			
			file << tab << tab << tab << "{ \"" << method.get("name") << "\", " << method.get("name") << "_args }," << endl;
		}

		file << tab << tab << tab << "{ 0, 0 }" << endl
		     << tab << tab << "};" << endl;

		file << tab << tab << "static ::DBus::IntrospectedProperty " << ifaceclass << "_properties[] = " << endl
		     << tab << tab << "{" << endl;

		for (Xml::Nodes::iterator pi = properties.begin(); pi != properties.end(); ++pi)
		{
			Xml::Node &property = **pi;

			file << tab << tab << tab << "{ "
				<< "\"" << property.get("name") << "\", "
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


		file << tab << tab << tab << "{ 0, 0, 0, 0 }" << endl
		     << tab << tab << "};" << endl;

		// generate the Introspected interface
		file << tab << tab << "static ::DBus::IntrospectedInterface " << ifaceclass << "_interface = " << endl
		     << tab << tab << "{" << endl
		     << tab << tab << tab << "\"" << ifacename << "\"," << endl 
		     << tab << tab << tab << ifaceclass << "_methods," << endl
		     << tab << tab << tab << ifaceclass << "_signals," << endl
		     << tab << tab << tab << ifaceclass << "_properties" << endl
		     << tab << tab << "};" << endl
		     << tab << tab << "return &" << ifaceclass << "_interface;" << endl 
		     << tab << "}" << endl
		     << endl;

		file << "public:" << endl
		     << endl
		     << tab << "/* properties exposed by this interface, use" << endl
		     << tab << " * property() and property(value) to get and set a particular property" << endl
		     << tab << " */" << endl;

		// generate the properties code
		for (Xml::Nodes::iterator pi = properties.begin(); pi != properties.end(); ++pi)
		{
			Xml::Node &property = **pi;
			string name = property.get("name");
			string type = property.get("type");
			string type_name = signature_to_type(type);

			file << tab << "::DBus::PropertyAdaptor< " << type_name << " > " << name << ";" << endl;
		}

		file << endl;

		file << "public:" << endl
		     << endl
		     << tab << "/* methods exported by this interface," << endl
		     << tab << " * you will have to implement them in your ObjectAdaptor" << endl
		     << tab << " */" << endl;

		// generate the methods code
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];
			Xml::Nodes args_in = args.select("direction","in");
			Xml::Nodes args_out = args.select("direction","out");

			file << tab << "virtual ";

			if (args_out.size() == 0 || args_out.size() > 1)
			{
				file << "void ";
			}
			else if (args_out.size() == 1)
			{
				file << signature_to_type(args_out.front()->get("type")) << " ";
			}

			file << method.get("name") << "(";
			
			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				file << "const " << signature_to_type(arg.get("type")) << "& ";

				string arg_name = arg.get("name");
				if (arg_name.length())
					file << arg_name;

				if ((i+1 != args_in.size() || args_out.size() > 1))
					file << ", ";
			}

			// generate the method 'out' variables if multibe 'out' values exist
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

					if (i+1 != args_out.size())
						file << ", ";
				}		
			}
			file << ") = 0;" << endl;
		}

		file << endl
		     << "public:" << endl
		     << endl
		     << tab << "/* signal emitters for this interface" << endl
		     << tab << " */" << endl;

  		// generate the signals code
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			file << tab << "void " << signal.get("name") << "(";

			// generate the signal arguments
			unsigned int i = 0;
			for (Xml::Nodes::iterator a = args.begin(); a != args.end(); ++a, ++i)
			{
				Xml::Node &arg = **a;

				file << "const " << signature_to_type(arg.get("type")) << "& arg" << i+1;

				if (i+1 != args.size())
					file << ", ";
			}

			file << ")" << endl
			     << tab << "{" << endl
			     << tab << tab << "::DBus::SignalMessage sig(\"" << signal.get("name") <<"\");" << endl;;


			if (args.size() > 0)
			{
				file << tab << tab << "::DBus::MessageIter wi = sig.writer();" << endl;

				for (unsigned int i = 0; i < args.size(); ++i)
				{
					file << tab << tab << "wi << arg" << i+1 << ";" << endl;
				}
			}

			file << tab << tab << "emit_signal(sig);" << endl
			     << tab << "}" << endl;
		}

		file << endl
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

			file << tab << "::DBus::Message " << stub_name(method.get("name")) << "(const ::DBus::CallMessage &call)" << endl
			     << tab << "{" << endl
			     << tab << tab << "::DBus::MessageIter ri = call.reader();" << endl
			     << endl;

			// generate the 'in' variables
			unsigned int i = 1;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				file << tab << tab << signature_to_type(arg.get("type")) << " argin" << i << ";"
				     << " ri >> argin" << i << ";" << endl;
			}

			if (args_out.size() == 0)
			{
				file << tab << tab;
			}
			else if (args_out.size() == 1)
			{
				file << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout1 = ";
			}
			else
			{
				unsigned int i = 1;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					file << tab << tab << signature_to_type(arg.get("type")) << " argout" << i << ";" << endl;
				}		
				file << tab << tab;
			}

			file << method.get("name") << "(";

			for (unsigned int i = 0; i < args_in.size(); ++i)
			{
				file << "argin" << i+1;

				if ((i+1 != args_in.size() || args_out.size() > 1))
					file << ", ";
			}

			if (args_out.size() > 1)
			for (unsigned int i = 0; i < args_out.size(); ++i)
			{
				file << "argout" << i+1;

				if (i+1 != args_out.size())
					file << ", ";
			}

			file << ");" << endl;

			file << tab << tab << "::DBus::ReturnMessage reply(call);" << endl;

			if (args_out.size() > 0)
			{
				file << tab << tab << "::DBus::MessageIter wi = reply.writer();" << endl;

				for (unsigned int i = 0; i < args_out.size(); ++i)
				{
					file << tab << tab << "wi << argout" << i+1 << ";" << endl;
				}
			}

			file << tab << tab << "return reply;" << endl;

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
