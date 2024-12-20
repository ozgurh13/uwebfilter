# TODO:
#     - copy over config file to /etc/uwebfilterd/config
#     - make procd file for service

include $(TOPDIR)/rules.mk

PKG_NAME       :=  uwebfilterd
PKG_VERSION    :=  1.0.0
PKG_SOURCE     :=  $(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_LICENSE    :=  BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/uwebfilterd
	SECTION       :=  net
	CATEGORY      :=  Network
	SUBMENU       :=  Firewall
	TITLE         :=  uwebfilterd
	DEPENDS       :=  +libnetfilter-queue +libnfnetlink +libmnl      \
	                  +libsqlite3 +libcurl +libjson-c
endef

define Package/uwebfilterd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uwebfilterd $(1)/usr/bin/
endef

$(eval $(call BuildPackage,uwebfilterd))
