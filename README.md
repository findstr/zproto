# zproto
A simple protocol buffer for lua

## Description

- Struct consist of members, the type of which can be basic type or other struct type
- Basic type can be 'boolean' or 'integer' or 'long' or 'string'
- All comment line begin with '#'
- All struct or field name begin with 'a-zA-Z'
- All field name must be defined after the character '.'
- Struct or field name and field tag must be unique in it's scope
- Field tag must large then 0 and defined by ASC
- When the suffix of type(include struct and basic type) is [], it means this field is array
- Struct can be defined as follows:

		#only the struct which has no parent can specify the protocol
		#protocol is aka struct name when it's explicitly specified
		#protocol is option and default value is '0'
		#protocol value can be queryed by zproto:tag@zproto.lua
		name [protocol] {
			.name:type 1
			...
		}

- A sample protocol define may like this:

		#comments line
		info {
			.name:string 1
			.age:integer 2
			.girl:boolean 3
		}
		packet 0xfe {
			phone {
				.home:integer 1
				.work:long 2
			}
			.phone:phone 1
			.info:info[] 2
			.address:string 3
			.luck:integer[] 4
		}

##luabind

This code is a simply lua binding of zproto in luabind folder

It provide a simple test code and you can run it as follow:

- Download lua-5.3 or later and extract into luabind/lua53
- Execute 'make linux' or 'make macosx' to build zproto.so
- Execute './test' to run the test code

##cppbind and csbind

This code is a simply [C++|C#] binding of zproto in [cppbind|csbind] folder

It also provide a simple test code and you can run it as follow:

- Execute 'make' to build a executable file named 'zproto',
	'zproto' used to generate [C++|C#] bind code from a file defined as zproto syntax
- Execute './zproto test.zproto' to generate [C++|C#] bind code for test.zproto
- Execute 'make test' to build a test program
- Execute './test' to run the test code.

