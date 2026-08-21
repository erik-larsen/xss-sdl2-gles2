# Bundled image set for the image-grabbing hacks

All images are **CC0 / public domain** (license verified via the
[Openverse](https://openverse.org/) API at fetch time; per-image
records below). They are served to the grab-ximage / grabclient hacks
(jigsaw, antspotlight, decayscreen, ripples, ...) in place of the
SMPTE colour-bars fallback. Resized to <=1600px wide, JPEG q82.

| file | title | creator | source | license | origin |
|------|-------|---------|--------|---------|--------|
| coast.jpg | Ocean Coast | Matt Bango | stocksnap | cc0 1.0 | https://stocksnap.io/photo/ocean-coast-TNCJQJEYXS |
| lake.jpg | Lake Reflection | Tim Sullivan | stocksnap | cc0 1.0 | https://stocksnap.io/photo/lake-reflection-VJDHGHXWF1 |
| clouds.jpg | Sunset Sky | Joshua Earle | stocksnap | cc0 1.0 | https://stocksnap.io/photo/sunset-sky-Q16XY4FN0F |
| waterfall.jpg | Pitniani waterfall | unknown | wikimedia | cc0 1.0 | https://commons.wikimedia.org/w/index.php?curid=49233971 |
| canyon.jpg | Antelope canyon rock formations | unknown | rawpixel | cc0 1.0 | https://www.rawpixel.com/image/3298937/free-photo-image-landscape-best-stone-pictures-images-bizarre |
| winter.jpg | Shelter snow, winter landscape | unknown | rawpixel | cc0 1.0 | https://www.rawpixel.com/image/6040562/photo-image-public-domain-tree-plant |
| river.jpg | File:North Saskatchewan River Valley from Highlands Edmonton Alberta Canada 01A.jpg | WinterE229 WinterforceMedia | wikimedia | cc0 1.0 | https://commons.wikimedia.org/w/index.php?curid=11067547 |
| dunes.jpg | Free desert dunes sand fabric | unknown | rawpixel | cc0 1.0 | https://www.rawpixel.com/image/5913109/photo-image-public-domain-fabric-free |
| wave.jpg | File:Riding the North Beach waves (Unsplash).jpg | Julie Macey jules144 | wikimedia | cc0 1.0 | https://commons.wikimedia.org/w/index.php?curid=58831834 |
| autumn.jpg | Autumn Forest Leaves Tones | unknown | rawpixel | cc0 1.0 | https://www.rawpixel.com/image/5970041/autumn-forest-leaves-tones |
| hills.jpg | Lush green rolling hills farmland | unknown | rawpixel | cc0 1.0 | https://www.rawpixel.com/image/11524311/photo-image-cloud-plant-grass |

Total: 11 images, ~2543 KB.

To add more: pick CC0 results from the Openverse API
(`https://api.openverse.org/v1/images/?license=cc0&q=...`), record the
provenance row here, resize to <=1600px, and drop the .jpg in this
directory -- both native builds and scripts/deploy-web.sh pick up the
directory contents automatically.
