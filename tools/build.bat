gcc -L../lib -I../lib smbusb_scan.c -o smbusb_scan.exe -lsmbusb
gcc -L../lib -I../lib smbusb_sbsreport.c -o smbusb_sbsreport.exe -lsmbusb
gcc -L../lib -I../lib smbusb_bq8030flasher.c -o smbusb_bq8030flasher.exe -lsmbusb
gcc -L../lib -I../lib smbusb_r2j240flasher.c -o smbusb_r2j240flasher.exe -lsmbusb