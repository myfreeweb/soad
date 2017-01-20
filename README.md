# soad: SOcket Activator/Deactivator [![unlicense](https://img.shields.io/badge/un-license-green.svg?style=flat)](http://unlicense.org)

A simple modern tool for **running web applications on demand and stopping them after a period of inactivity**.


Back in the day, web apps used something like CGI or inetd, where one request got one UNIX process that would, well, process it — and quit. This meant that your whole app would be loaded and unloaded once per request, which is especially slow when you use big frameworks written in scripting languages. But it also meant that when no requests were being processed, no resources (such as RAM) were held. Of course, these days no one cares about not holding resources and this approach is completely dead [OH WAIT NO IT'S THE COOL NEW THING](https://github.com/anaibol/awesome-serverless).

But what I want to do is to [run many rarely used services on a VPS with low RAM](http://www.urbandictionary.com/define.php?term=clown%20computing). Modern web app servers are designed to run on a socket until you stop them. So what if I want to stop inactive services?

## Installation

Should work on any modern UNIX-like system.

```bash
$ cc -lpthread -o soad soad.c
```

And put `soad` where you keep your binaries.

## Usage

First, your web server needs to support socket activation.
Either by explicitly specifying a file descriptor via something like an `fd://3` argument, or by using the tiny metadata protocol from systemd — the `LISTEN_PID`, `LISTEN_FDS` and `LISTEN_FDNAMES` environment variables.
It also needs to shut down gracefully on `SIGTERM`.

Some libraries and apps for that:

- Ruby: [Puma](https://github.com/puma/puma/blob/master/docs/systemd.md#socket-activation) (see examples below. tl;dr needs the same socket path specified, it literally checks the path)
- Python: [gunicorn](http://docs.gunicorn.org/en/stable/deploy.html?highlight=activation#systemd)
- Python/Lua/Perl/Ruby/etc.: [uWSGI](http://uwsgi-docs.readthedocs.io/en/latest/Systemd.html) (see examples below. tl;dr needs an obscure option to shut down gracefully on SIGTERM. it can also [do this sort of thing on its own](http://uwsgi-docs.readthedocs.io/en/latest/OnDemandVassals.html), but in a more complex and memory consuming way)
- Node.js: [socket-activation](https://github.com/sorccu/node-socket-activation) (note: needs a socket name. obviously, it is `soad`)
- Go: [go-systemd/activation](https://github.com/coreos/go-systemd/tree/master/activation) or [systemd.go](https://github.com/lemenkov/systemd.go); [+ manners](https://github.com/braintree/manners) for graceful shutdown, see [example](https://github.com/myfreeweb/classyclock/blob/328ab8378c19455a7eaaee7fafef7c5eb28f8526/web-app.go#L32-L62)
- Rust: [systemd_socket](https://github.com/viraptor/systemd_socket)
- Haskell: [socket-activation](https://github.com/ddfisher/haskell-socket-activation) ([+ Warp graceful shutdown example](https://github.com/myfreeweb/sweetroll/blob/c48a29b3b4f2b666c479df45908fbd22b05da88f/executable/Main.hs#L78-L96))

(You can implement your own in 5 minutes, all you need to do is create a socket object from a file descriptor. See `test_server.rb` for a tiny example.)

Now, to run your app on a UNIX domain socket, something like this:

```bash
$ soad -s /var/run/myapp.sock -t 240 -- puma -b unix:/var/run/myapp.sock
$ soad -s /var/run/myapp.sock -t 240 -- gunicorn app:app
$ soad -s /var/run/pyapp.sock -t 240 -- uwsgi --master --hook-master-start "unix_signal:15 gracefully_kill_them_all" --wsgi-file app.py --callable app --lazy-apps
```

The `-t`/`--time-until-stop` argument is the number of seconds the app will be allowed to run without any activity.
(Activity is determined by incoming socket connections, so basically the number of seconds since the last incoming request.)

Point your reverse proxy to that socket and enjoy.

## Contributing

Please feel free to submit pull requests!

By participating in this project you agree to follow the [Contributor Code of Conduct](http://contributor-covenant.org/version/1/4/).

## License

This is free and unencumbered software released into the public domain.  
For more information, please refer to the `UNLICENSE` file or [unlicense.org](http://unlicense.org).
