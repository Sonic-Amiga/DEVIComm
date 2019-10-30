# Introduction

This project is a demostration application, which connects to DEVIReg Smart
floor heating thermostat (https://www.devismart.com/ and is able to dump state
and send commands.

# Legal notice

This project is based on original demo code, released on github by Trifork
(https://github.com/trifork/secure-device-grid). According to their license,
this code and accompanying library in binary form can be used for evaluation
purposes only. Using it in a produce requires a license. Original description
from original authors is preserved in README-original.md file, please refer for
more information.

## How to use

1. Build and run the demo application.
2. On your smartphone or tablet in DEVIReg<sup>TM</sup> Smart application choose
   "Share house" function. Answer anything to the question whether the new user
   is allowed to add or delete users and get the pairing code.
3. In the demo app command line execute "/pair <OTP code>" command. Use the code,
   supplied by the DEVIReg<sup>TM</sup> Smart application, enter it as just\
   numbers without dashes.
4. The demo application will receive thermostat IDs from your smartphone. The
   smartphone will also take care about registering the new user on thermostats.
   The demo application presents itself as "DEVIComm test" user. You will be
   able to list them using "/lp" command. It will also remember ID of the smartphone.
5. Execute "/pcr <thermostat-peer-id>" command in order to connect to a thermostat.
   After successful connection the demo application will decode and display all the
   incoming data. The thermostat automatically fully dumps its configuration and
   then keeps on sending updates, if there are any.
6. At the moment you can only send raw hexadecimal commands to the thermostat using
   "/send-hex" command. However i have not tested it. Be careful and know what you
   are doing. I have a suggestion that it is extremely easy to crash the thermostat
   and even perhaps brick it by carelessly doing a wrong thing, like buffer overflow.
