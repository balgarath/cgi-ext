= cgiext

* http://github.com/balgarath/cgi-ext/tree/master

== DESCRIPTION:

* This is a C extension of some of the functions of Ruby's cgi.rb

== FEATURES/PROBLEMS:

* Haven't figured out how to get around this yet, but to get cgiext 
working in rails, you need to add the following to 
ApplicationController:
session :cookie_only => false

otherwise rails will throw a sessionfixationattempt error


== SYNOPSIS:

  FIX (code sample of usage)

== INSTALL:

* sudo gem install cgiext
* require 'cgiext' (if a rails project, add this to 
production/development.rb, whichever)


== LICENSE:

Ruby License
