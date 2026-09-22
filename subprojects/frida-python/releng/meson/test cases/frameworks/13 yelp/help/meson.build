gnome = import('gnome')

gnome.yelp('meson',
   sources: 'index.page',
   media: 'media/test.txt',
   symlink_media: false,
   languages: ['de', 'es'],
)

gnome.yelp('meson-symlink',
   sources: 'index2.page',
   media: 'media/test.txt',
   symlink_media: true,
   languages: ['de', 'es'],
)

gnome.yelp('meson-linguas',
   sources: 'index3.page',
   media: 'media/test.txt',
   symlink_media: false,
)
