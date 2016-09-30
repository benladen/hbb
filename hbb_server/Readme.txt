Install/First Run
---------------------
Replace <IP ADDRESS HERE> in hbb_cfg.py with the IP address which the server will be accessible from.
After running hbb_cfg.py, put the new config.dat into the data folder inside hbb.vpk

Edit config.ini however you want, using the existing values as examples.

Place VPK files into the data directory.

Run hbb_srv, then visit it in the browser.
Such as if HTTP_Port=40222 and you are running it on your computer, then you can go to http://127.0.0.1:40222/

Use "Add entry to the app table." until everything is added.
The web interface will not tell you any details about errors (or if an error even happened at some points),
Check the results in "Display all in the app table."

Test hbb.vpk to verify your config.dat

If you want it to be accessible from the internet, you should have a static IP which you would be using in hbb_cfg.py
and the port must be forwarded in your firewall. Do not forward the HTTP_Port unless you want that to be accessible too.



Security
---------------------
Run at your own risk.



Required Libraries
---------------------
zlib 1.2.8
libpng 1.2.56