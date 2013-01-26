mod_fastfile.la: mod_fastfile.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_fastfile.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_fastfile.la
