# include this at the end of every Makefile

$(DEPDIR)/%.d: ;

-include $(wildcard $(DEPDIR)/*.d)
