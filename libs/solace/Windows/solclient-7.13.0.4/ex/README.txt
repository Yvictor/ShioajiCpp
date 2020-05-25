	Solace Corporation Messaging API for C Sample Programs

INTRODUCTION

   These samples provide a basic introduction to using the Messaging API for
   C in messaging applications. Common uses, such as sending a message,  
   receiving a message, asynchronous messaging, and subscription management,
   are described in detail in these samples.

   Before working with these samples, ensure that you have read and understood
   the  basic  concepts  found in the Messaging APIs Developer Guide.
   
SAMPLE DOCUMENTATION

   For a description of each sample and an annotated listing of the source code 
   please see the "C CSMP API Documentation" included in solclient-doc.tar.gz.


HOW TO BUILD THE SAMPLES

   REQUIRED SOFTWARE:

   To compile and run the sample applications on Windows systems, ensure that 
   Microsoft Visual Studio 2008 Standard Edition or above is installed and configured 
   on your computer. Be sure you have installed the correct Solace Corporation package 
   based on your installed Visual Studio.

   For Visual Studio 2008, 2010, 2012/2013 install solclient_Win_<version>.tar.gz

   For  Visual Studio 2015 install solclient_Win_vs2015_<version>.tar.gz

WINDOWS BUILD INSTRUCTIONS

Visual Studio 2008:

   1. Navigate to the 'vs2008' directory. If there is just a vs2015 directory and
      no vs2008 directory, you have installed the wrong package.
   2. Open 'c_sdk_examples.sln' in Microsoft Visual Studio.

Visual Studio 2010:
   1. Navigate to the 'vs2010' directory. If there is just a vs2015 directory and
      no vs2010 directory, you have installed the wrong package.
   2. Open 'c_sdk_examples.sln' in Microsoft Visual Studio.

Visual Studio 2012/2013:
   1. Navigate to the 'vs2010' directory. If there is just a vs2015 directory and
      no vs2010 directory, you have installed the wrong package.
   2. Open 'c_sdk_examples.sln' in Microsoft Visual Studio, and upgrade the solution 
      to vs2012/2013.


Once you have Visual Studio open, the build procedure is the same on all
platforms:

   3. Change to the relevant build configuration and platform.  Build configurations 
      available  are:
  	a. 'Release' for optimized build
	b. 'StaticRelease ' for optimized build with a static library (.lib instead of .dll)
	c. 'Debug' for debug build.
	d.  'StaticDebug' for debug build with a static library.

      Platform options are:
	a. Win64 for 64 bit builds.
	b. Win32 for 32 bit builds.

   4. Build the solution 'c_sdk_examples'.



Visual Studio 2015:
   1. Navigate to the 'vs2015' directory. If there is a vs2008 directory and
      a vs2010 directory but no vs2015 directory, you have installed the wrong package.
   2. Open 'c_sdk_examples.sln' in Microsoft Visual Studio.
   3. Change to the relevant build configuration and platform.  Build configurations 
      available  are:
	a. 'Release' for optimized build
	b. 'StaticRelease ' for optimized build with a static library (.lib instead of .dll)
	c. 'Debug' for debug build.
	d.  'StaticDebug' for debug build with a static library.

      Platform options are:
	a. Win64 for 64 bit builds.
	b. Win32 for 32 bit builds.

   4. Build the solution 'c_sdk_examples'.


RUNNING THE EXAMPLES

   All of the supplied examples have command line help. Run the corresponding
   executable with no arguments to display its usage

USING THE SUPPLIED OPENSSL LIBRARIES

   The OpenSSL binaries (i.e. libeay32.dll ssleay32.dll) provided with the API
   depend on the Microsoft Visual C++ Redistributable binaries.  If you try to
   use SSL and see an error that the microsoft visual studio runtime library is 
   missing (EG. msvcr100.dll or vcruntime140.dll), consider one of the
   following options:
	1. Obtain the appropriate Microsoft Visual C++ Redistributable package.  It is
  	   available from: http://www.microsoft.com/en-us/download/details.aspx?id=8328
	2. Obtain a different build of the library, which may depend on a version of the
  	   Microsoft Visual C++ runtime that you may already have.  For options, see:
  	   http://www.openssl.org/related/binaries.html
	3. Build the OpenSSL library from source, using the runtime version of your
  	   choice.



Copyright 2009-2020 Solace Corporation. All rights reserved.

This software is proprietary software of Solace Corporation and intended only
for use in conjunction with one or more Solace Message Routers.  By using this
software, you are agreeing to the license terms and conditions located at
https://solace.com/license-software.

