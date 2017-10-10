#
# Component Makefile
#

ifdef CONFIG_ETHERNET_ENABLE
COMPONENT_SRCDIRS := . eth_phy
else
COMPONENT_SRCDIRS :=
endif
