# winddcutil
An partial implementation of https://github.com/rockowitz/ddcutil for Windows. Winddcutil can communicate with attached monitors using Display Data Channel/Command Interface (DDC/CI) to query and change monitor state, f.e. to control which input source is displayed.

## Features
- List attached displays
- Query display capabilitites string
- Set VCP feature values

## Usage
~~~:
winddcutil <command>
~~~
Where command is one of:
~~~:
detect
capabilities <display ID>
setvcp <display ID> <feature-code> <new-value>
~~~

## Example
List available displays:
~~~:
C:\>winddcutil detect
Display 0
         Name: Dell P2416D(HDMI)
Display 1
         Name: Generic PnP Monitor
~~~
Show capabilities of display 0:
<pre>
C:\>winddcutil capabilities 0
(prot(monitor)type(LCD)model(P2416D)cmds(01 02 03 07 0C E3 F3)
vcp(02 04 05 08 10 12 14(05 08 0B 0C) 16 18 1A 52 <b>60(01 11 0F)</b>
AA(01 02) AC AE B2 B6 C6 C8 C9 D6(01 04 05) DC(00 02 03 05) DF
E0 E1 E2(00 01 02 04 0E 12 14 19) F0(00 08) F1(01 02) F2 FD)
mswhql(1)asset_eep(40)mccs_ver(2.1))
</pre>
Set VCP feature 96 (0x60) to value 15 (0x0F):
~~~:
C:\>winddcutil setvcp 0 96 15
~~~
Feature 0x60 is the selected input source. Setting it to 0x0F selects the first display port, at least for this monitor. Available input sources are shown above in bold.

## TODO
(in no particular order)
- Parse capabilities string, to create output similar to the original ddcutil.
- Accept numeric parameters in hexadecimal.
- Implement more features, such as querying current VCP feature values.
