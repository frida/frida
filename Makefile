all $(MAKECMDGOALS):
	@build_os=$$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,'); \
	$(MAKE) -f Makefile.$$build_os.mk $(MAKECMDGOALS)

.PHONY: all $(MAKECMDGOALS)
