% type(3) 1.0 | Expose custom Tcl_ObjTypes to Tcl script
% Cyan Ogilvie
% 1.0

# NAME

type - Allow Tcl script to create custom Tcl_ObjTypes

# SYNOPSIS

**package require type** ?1.0?

**type::define** *typename* *handlers*  
**type::get** *type* *value*  
**type::with** *varname* *type* *script*  

# DESCRIPTION

Exposes the ability to define custom Tcl_ObjTypes to Tcl scripts, implemented
by Tcl callbacks.

# COMMANDS

**type::define** *typename* *handlers*
:   Define a new type called *typename*, or redefine it if it already exists,
    implemented by callbacks supplied in the dictionary *handlers*.  Each
    callback is a Tcl list to which arguments will be appended and the result
    evaluated in the global namespace.  The callbacks are:

    **create**
    :   Called to construct the type's intrep given the value.  Must return
        The value to be stored as the intrep, or throw an exception if an
        intrep for *type* could not be created from the value.  Required for
        all types.

    **string**
    :   Called to construct the string representation given the intrep.
        May be omitted if the intrep is never modified directly (and so the
        string representation is never invalidated).

    **free**
    :   Called when the value is being freed.  Must release any resources
        used in the intrep.  May be omitted if the intrep does not contain
        any resources that need to be explicitly released.

    **dup**
    :   Called to duplicate an intrep.  Called with the intrep to duplicate,
        and must return a new intrep such that changes to the new intrep do
        not affect the old one.  May be omitted if the intrep is never
        modified directly (and so never needs to be copied).

    The argument appended to **string**, **free** and **dup** is the intrep
    to operate on.  The **create** callback will have the value and type context
    appended (see the **TYPE CONTEXT** section for a description of that).

**type::get** *type* *value*
:   Retrieve read-only access to the intrep of type *type* for the value
    *value*, creating it if it didn't exist.  Returns the intrep or throws
    an error if an intrep for that type didn't exist and couldn't be created
    (likely because *value* is not a valid string representation for that
    type).  While this extension has no way to enforce the read-only
    restriction for the returned intrep, scripts must ensure that contract
    is honoured or bad things will happen - the reference-counting based
    copy on write semantics that underpin the Tcl value model will break
    and other users of that value will see their copies changing unexpectedly.
    The string representation will also go out of sync with the intrep.

**type::with** *varname* *type* *script*
:   Retrieve an unshared, writable intrep of type *type* from the value stored
    in the variable *varname* and evaluate *script*.  While *script* is
    executing *varname* contains the writable intrep.  Once *script* finishes,
    *varname* again contains the value and the intrep is no longer accessible.
    This command invalidates the string representation of the value in *var*,
    and duplicates the intrep if value was shared, calling the *dup* type
    handler if one is defined.

    Throws an exception if the value in *var* isn't of type *type* and can't be
    converted to one, or if *type* doesn't define *string* or *dup* handlers.

# TYPE CONTEXT

Sometimes a type includes some context for which the intrep is valid, beyond
what is captured in the value itself.  Examples of this include the interpreter
instance and compilation epoch for the bytecode type, and the table for which
an intrep is an index into for the type used by Tcl_GetIndexFromObj.  If an
intrep for that type is requested for the same value but a different context,
the existing intrep for that type is no longer valid and a new one must be
generated.

To support this pattern in this Tcl interface, the *type* argument to
**type::get** and **type::with** is a list, the first element being the
*typename*, matching the value supplied to the corresponding
**type::define** call, and the second being some value that captures the
context for which the intrep is valid.  This context element is optional and
only needs to be supplied if the intrep validity is conditional on some context
beyond the value itself.

If the *type* argument contains a context element, it is appended as a second
argument to the *create* callback, after the value.

# TYPE SCOPE

Types defined by **type::define** are only valid for and visible to the
interpreter that the **type::define** command was run in.

There is no "undefine" command, since this would orphan existing values of
that type, stranding their intreps with no way to free their resources.  It may
be possible to construct a reference counting scheme to free the handlers when
the last value of that type goes away, but it isn't clear that there is a use
case that justifies the complexity that would add.

# HANDLER EXCEPTIONS

Exceptions may be thrown by the **create** handler, and indeed must be if the
value is not valid for that type, but throwing an exception from the other
handler callbacks will result in a Tcl_Panic, since the failure of those
callbacks to honour their contract leaves the interpreter in an undefined
state, and occurs in a context that cannot report errors.  Great care must
therefore be taken in implementing these callbacks to ensure that they
can't throw exceptions and are always able to fulfill their obligations.

# EXAMPLES

A simple example of a read-only type used as a cache to store some value
computed from a string representation, in this case a sha1 hash of the value:

~~~tcl
package require sha1

type::define sha1 {
    create sha1::sha1
}

set greeting "hello, world"

# Retrive the sha1 type's intrep, creating it here
# since it doesn't exist yet
set hash1   [type::get sha1 $greeting]   ;# Takes 73 µs

# Retrieve the sha1 intrep again, this time the intrep exists
# (from the above request for it), and so it is just
# returned directly
set hash2   [type::get sha1 $greeting]   ;# Takes 0.7 µs

# Display the intrep info: produces output like:
# value is a objtype_sha1 with a refcount of 4,
# object pointer at 0x1e2ace0, internal representation
# 0x1ebb2c0:0x0, string representation "hello, world"
puts [tcl::unsupported::representation $greeting]
~~~

Since this cached sha1 hash is stored as the intrep of the value in the
greeting variable, it will be freed when that value is no longer referenced:
when it goes out of scope, or the greeting variable is changed to store
a new value, and so caches implemented using this pattern are implicitly
managed and don't need cache expiry or cache size management to avoid
unbounded growth.  The cached values that exist at any time are only those
that are currently in use for the working set of values.

## Mutating the Intrep

As a more involved example, this provides a tdom document as an intrep
for an XML value, allowing the dom to be mutated directly and regenerating
the string representation as needed, and freeing the dom when the value
is released:

~~~tcl
package require tdom

type::define XML {
    create  {apply {val {
        dom parse $val
    }}}
    string  {apply {intrep {
        $intrep asXML
    }}}
    dup     {apply {intrep {
        dom parse [$intrep asXML]
    }}}
    free    {apply {intrep {
        $intrep delete
    }}}
}

set xml {<foo><bar>Bar text</bar><baz>Baz text</baz></foo>}

# Returns "Baz text"
[type::get XML $xml] selectNodes string(//baz)

type::with xml XML {
    [$xml documentElement] setAttribute something changed
}

puts $xml
~~~

Produces:

~~~xml
<foo something="changed">
    <bar>Bar text</bar>
    <baz>Baz text</baz>
</foo>
~~~

## Using Type Context

As an example using type context, this emulates the Tcl_GetIndexFromObj
functionality in a Tcl script (including the non-empty, non-ambiguous
prefix matching), that saves the index it found into the intrep for reuse
later if called with the same value and table.  If the table changes, the
cached index is no longer valid, even though the type and value haven't
changed, and so the index has to be recomputed:

~~~tcl
type::define tableindex {
    create {apply {{val table} {
        set preflen	[string length $val]
        if {$preflen == 0} return
        set i	-1
        lmap e $table {
            incr i
            if {[string range $e 0 $preflen-1] ne $val} continue
            set i
        }
    }}}
}

proc getindex {str table msg} {
    set matches	[type::get [list get-4.1 $table] $str]
    switch -- [llength $matches] {
        1 {lindex $matches 0}
        default {
            throw [list TCL LOOKUP INDEX $msg $str] \
                "bad $msg \"$str\": must be [switch [llength $table] {
                    1 {lindex $table 0}
                    2 {join $table { or }}
                    default {
                        string cat \
                            [join [lrange $table 0 end-1] {, }] \
                            ", or [lindex $table end]"
                    }
                }]"
        }
    }
}

set subcommands {
    args
    body
    class
    cmdcount
    commands
    complete
    coroutine
    default
    errorstack
    exists
    frame
    functions
    globals
    hostname
    level
    library
    loaded
    locals
    nameofexecutable
    object
    patchlevel
    procs
    script
    sharedlibextension
    tclversion
    vars
}

set switches {
    -exact
    -glob
    -regexp
    -sorted
    -all
    -inline
    -not
    -start
    -ascii
    -dictionary
    -integer
    -nocase
    -real
    -decreasing
    -increasing
    -bisect
    -index
    -subindices
}

set c	host

# Returns 13 - the index into $subcommands that
# matches the prefix "host"
getindex $c $subcommands subcommand

# Throws an exception in the style of Tcl_GetIndexFromObj,
# even though $c is of the right type and has an index
# intrep: the type context (the table in this instance) is
# not the same, so the lookup needs to be redone against
# the new table.  "host" is not a valid prefix for any
# of the entries in $switches, and so the appropriate
# exception is thrown.
getindex $c $switches switch
~~~

Scanning the table for matches for the supplied prefix is relatively expensive
(about 26 µs in the successful case above), and so caching the index is a
worthwhile optimization, bringing the time for subsequent calls to getindex
down to 0.8 µs.

# CALLING AS ENSEMBLE

While just syntactic sugar, it is often more pleasant to expose namespace
commands as an ensemble, so that **type::define** can be called as
**type define**.  This package supports this calling style, but since fetching
an intrep from a value that already has one for the requested type is so fast,
the overhead of the ensemble dispatch relative to directly calling the command
roughly doubles the cost of the call (0.2 µs to 0.4 µs).

# DISCUSSION

One of the really distinctive features of Tcl is the way it handles values -
from the script point of view all values are strings (EIAS), but to avoid the
performance penalty this would imply if implemented naively, efficient internal
representations of values are created and attached to the values as needed.  In
the most general case, a Tcl value can have a string representation, a
type-dependent internal representation for efficiently treating that value as
some type, or both.  This scheme may be extended in the future to allow more
than one type's internal representation to be attached to a value, addressing
a performance problem called *shimmering* - when a value is used as two or more
types (in addition to a plain string) alternately in a repeated fashion,
causing each internal representation to be constructed and then discarded, only
to be rebuilt when that value is used as that type again.  Approaches to fix
this are usually referred to as a Hydra - a many headed value.

The design philosophy in which all values are fully defined by a string
representation (usually called EIAS - Everything Is A String), has interesting
consequences - serialization is trivial for all value types, making it easy
to transfer arbitrarily complex values over a network socket for instance,
or printing them out to debug program state, or persisting them to disk.  It
also allows unique interfaces for working with things like JSON or XML, which
are string representations of structured data.  Commands can be provided that
act on the value itself, rather than having to convert the value to some native
representation, operate on it, then convert it back.  This remains efficient
because a native representation of the structured data is created on demand,
and preserved with that value for future use.  It is also frequently used as a
cache of some value computed from a string - the compiled bytecodes for the
script body of a procedure, or the index into a table of strings for some
string (i.e. Tcl_GetStringFromIndex), so that subsequent uses of that value in
that context can skip the scan and just return the index directly, making a
frequently used pattern very fast.

This combines with the reference counting and copy-on-write mechanisms of
Tcl values to provide an enormous performance benefit while retaining a very
simple programming model that allows novel solutions to problems.

Since its introduction in Tcl 8.0 it has only been available to the Tcl core
or extensions written in C or a similar (non-Tcl) language that interfaces
with the Tcl C API.  This extension exposes the feature to Tcl scripts, so
that new custom Tcl_ObjTypes can be defined and used entirely on the 
script layer, with greater ease and safety than implementing the type in C.

# TODO

Expose a stubs interface so that other extensions built in C can define custom
Tcl_ObjTypes in Tcl, and use them from C.  There are cases where implementing
the type in C would be tedious and error prone whereas the Tcl implementation
is almost trivial (such as the XML using tdom example above), and the
performance for retrieving an intrep is the same whether it was created in
C or in a Tcl callback, with the minor limitation that the value stored as the
intrep must be a Tcl_Obj when defined by Tcl.

# LICENSE

This package is Copyright 2020 Cyan Ogilvie, and is made available under the
same license terms as the Tcl Core.

# SEE ALSO

Tcl_RegisterObjType(3)
