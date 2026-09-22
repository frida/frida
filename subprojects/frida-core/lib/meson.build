if host_os == 'android'
  subdir('selinux')
endif
subdir('base')
subdir('netif')
subdir('pipe')
if have_in_tree_payloads
  subdir('payload')
endif
if have_local_backend
  subdir('agent')
endif
if have_in_tree_payloads and build_gadget
  subdir('gadget')
endif
