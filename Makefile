CC=gcc
CFLAGS=-O0 -g3 -Wall  
LDFLAGS=  -L/lib -L/local/samba/lib/ -I/local/samba/include/   -lnsl -lnetfs -lfshelp -liohelp -lpthread -lports -lihash -ldl -lshouldbeinlibc -lsmbclient

smbfs: clean smb.o smbfs.o smbnetfs.o
	$(CC) $(LDFLAGS) smb.o smbfs.o smbnetfs.o -osmbfs

smb.o:
	$(CC)  $(CFLAGS) smb.c  -I/local/samba/include/ -c  
        
smbfs.o:
	$(CC)   $(CFLAGS) smbfs.c  -I/local/samba/include/ -c  
        
smbnetfs.o:
	$(CC)   $(CFLAGS) smbnetfs.c  -I/local/samba/include/ -c  
        
clean:
	rm -rf *.o smbfs
        
all: smbfs
