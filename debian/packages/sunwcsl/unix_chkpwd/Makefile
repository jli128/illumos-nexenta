all: unix_chkpwd

unix_chkpwd.o: unix_chkpwd.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@

md5_good.o: md5.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHIGHFIRST -D'MD5Name(x)=Good##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_broken.o: md5.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Broken##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_crypt_good.o: md5_crypt.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Good##x' \
		$(TARGET_ARCH) -c $< -o $@

md5_crypt_broken.o: md5_crypt.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -D'MD5Name(x)=Broken##x' \
		$(TARGET_ARCH) -c $< -o $@

unix_chkpwd: md5_good.o md5_broken.o md5_crypt_good.o md5_crypt_broken.o unix_chkpwd.o bigcrypt.o

clean:
	rm -f *.o unix_chkpwd
