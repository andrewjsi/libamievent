# Asterisk Manager Interface client C library

libamievent is an asynchronous event-driven client library for Asterisk Manager
Interface. It uses [libev](http://software.schmorp.de/pkg/libev.html) as event
loop backend. 

With the libamievent you can send AMI commands and you can subscribe for
response to the command. When it arrives, the libamievent call the callback
function, which specified at subscription. The callback function allows you to
query AMI variables.

libamievent support AMI events. You can also specify a callback function of
what the libamievent is called when events are received.

## Requirements

* libev

For Debian users:

    apt-get install libev-dev

For Gentoo users:

    emerge -av libev

## Todo, Future

* more stability
* well-written documentation
* the ability to be integrated with other systems as easy as possible

## Source code

The source code is well written as far as possible. We do not use tabs, instead
of 4 spaces is indented. All identifiers, including variables, function names
and macros written in English, but some comments and commits in Hungarian is,
because we are speaking and thinking in Hungarian. Nevertheless, we try to
write everything in English in the future.

## Contribution

It is an open source project, which is to be better and better. If you have any
ideas or find an error, or get stuck, you can contact us, please file a bug
report or send a pull-request!

## License

[_GPL2_](https://www.gnu.org/licenses/gpl-2.0.html)

(C) Copyright 2010-2014 Andras Jeszenszky, [JSS & Hayer
IT](http://www.jsshayer.hu) All Rights Reserved.
