                Introduction.

  This folder contains partial source code for a simple VM, with few intentionally 
 missed pieces. This VM implements simple language (MVM language) with basic
 computational functionality and flow control.

                MVM language.

   Language has simple C-style syntax and is intentionally rather
 basic. Language has 3 types: 
     - 64-bit integer type, called 'int'
     - ANSI C compatible double type, called 'double'
     - immutable strings, called 'string' 
 
  Every variable is scoped, scope is marked with curly braces ({ and }).
 Variable has to be declared before first use, otherwise translation
 error will happen.

  Values in variable could be modified with traditional C-style
 expressions, like x= 2+y*3; with traditional C-style operators
 precedence, i.e. aforementioned expression is evaluated as 
  x= (2+(y*3));

  Variables in topmost scope could be bound to Var class
 instances, to allow interoperability between MVM programs and C++. 


 Literals can be:
   - integer, such as 42 or 123456789012345
   - double, such as 42.0 or 1e-18
   - string, such as '42' or 'Hello \'World\'\n'

 Flow control is as following:
    - 'for' loop, with semantics:
         for (i in -10..x) { print('i=', i,    '\n'); }
    - 'if' conditions, such as
         if (x == 8 && y <= 2) { 
             print('c1\n'); 
         } else {
             print('c2\n'); 
         }

               Implementation.
 
   We provide generic source -> AST (abstract syntax tree)
 translator, so that implementors can focus on VM-specific issues.
 It means we give scanner and top-down parser, generating tree
 of AST nodes, representing program structure.
 It is up to implementors to provide remaining pieces for fully
 functional VM.

                Tests.

  Directory 'tests' contains set of tests on basic language features,
 along with test driver 'run.py'. Generally, for every MVM program 
 we have an .expect file, which contains expected result of execution
 for given MVM program. By default, 'run.py' will run all tests it
 knows about.
 
  
                Source tree layout.

  Folder 'include' contains generic declarations of language constructs
 (as AST nodes), set of bytecodes with description, and certain basic
 interfaces.
  
  Folder 'tests' contains MVM tests. Feel free to implement your own tests.

  Folder 'vm' contain source code for the VM.

                Building.
  After cloning repository run 'git submodule init && git submodule update'.
  Build shall be pretty straightforward, just type 'make' in command
 line within `impl` directory (known to work with MacOS and Linux). Use 'make OPT=1' for an optimized build.
