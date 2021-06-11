## Installing 'constatus' systemd.service

1.  Build 'constatus' and install the binary
    (`/usr/bin/constatus` or `/usr/local/bin/constatus`).

2.  Create a configuration directory, and create
    a constatus configuration file:

        mkdir /etc/constatus
        cp constatus.cfg /etc/constatus

    Adjust the configuration file according to your system's cameras.

3.  Copy the `constatus.service` unit file to systemd's configuration
    directory. On Debian/Raspian, it is `/etc/systemd/system`.

        cp constatus.service /etc/systemd/system/

    Adjust the PATH of the constatus binary as needed (e.g. `/usr/local/bin`
    or `/usr/bin`).

4.  Reload the systemd services list:

        systemctl daemon-reload

5.  Start/stop the service (manually / one-time):

        systemctl start constatus.service
        systemctl stop constatus.service

6.  Enable/disable the service on boot time:

        systemctl enable constatus.service
        systemctl disable constatus.service

7.  Check status of the service:

        systemctl status constatus


## Troubleshooting

Start the service with `systemctl start constatus.service`.
Use `systemctl status constatus.service` to see any possible errors, e.g.:

```
● constatus.service - constatus
   Loaded: loaded (/etc/systemd/system/constatus.service; disabled; vendor preset: enabled)
   Active: failed (Result: exit-code) since Sat 2018-12-29 18:06:01 MST; 4min 0s ago
  Process: 5081 ExecStart=/usr/local/bin/constatus -c /etc/constatus/constatus.cfg (code=exited, status=1/FAILURE)
 Main PID: 5081 (code=exited, status=1/FAILURE)

Dec 29 18:06:01 myrpi constatus[5081]: errno: 16 (Device or resource busy)
Dec 29 18:06:01 myrpi constatus[5081]: Selected pixel format not available
Dec 29 18:06:01 myrpi constatus[5081]: errno: 16 (Device or resource busy)
Dec 29 18:06:01 myrpi constatus[5081]: 2018-12-29 18:06:01.846420  INFO constatus  Loading /etc/constatus/constatus.cfg...
Dec 29 18:06:01 myrpi constatus[5081]: 2018-12-29 18:06:01.862730  INFO constatus   *** constatus v2.0 starting ***
Dec 29 18:06:01 myrpi constatus[5081]: 2018-12-29 18:06:01.862987  INFO constatus  Generic resizer instantiated
Dec 29 18:06:01 myrpi constatus[5081]: 2018-12-29 18:06:01.863077  INFO constatus  Configuring the video-source...
Dec 29 18:06:01 myrpi systemd[1]: constatus.service: Main process exited, code=exited, status=1/FAILURE
Dec 29 18:06:01 myrpi systemd[1]: constatus.service: Unit entered failed state.
Dec 29 18:06:01 myrpi systemd[1]: constatus.service: Failed with result 'exit-code'.
```

In the above systemd output, lines starting with `constatus[5081]` are
the output/error messages from constatus itself. The same messages would
be reported if constatus is started manually:

```
# /usr/local/bin/constatus -c /etc/constatus/constatus.cfg
2018-12-29 18:12:27.174442  INFO constatus  Loading /etc/constatus/constatus.cfg...
2018-12-29 18:12:27.185545  INFO constatus   *** constatus v2.0 starting ***
2018-12-29 18:12:27.185847  INFO constatus  Generic resizer instantiated
2018-12-29 18:12:27.185956  INFO constatus  Configuring the video-source...
Selected pixel format not available
errno: 16 (Device or resource busy)
```

This is a good sign - as it means the systemd infrastructure is working well.
Now you'll need to fix the configuration file (`/etc/constatus/constatus.cfg`)
until constatus starts successfully.

## More information

For more information about systemd on Debian see:
<https://wiki.debian.org/systemd/Services>,
<https://wiki.debian.org/systemd#Creating_or_altering_services>.
