# QUIET Icon Files

This directory should contain the following icon files for the installer:

## Windows Icons
- `icon.ico` - Windows icon file (should contain 16x16, 32x32, 48x48, 256x256 sizes)

## macOS Icons  
- `icon.icns` - macOS icon file (should contain all required sizes for Retina displays)

## PNG Icons (for Linux and general use)
- `icon_16.png` - 16x16 pixels
- `icon_32.png` - 32x32 pixels
- `icon_48.png` - 48x48 pixels
- `icon_64.png` - 64x64 pixels
- `icon_128.png` - 128x128 pixels
- `icon_256.png` - 256x256 pixels
- `icon_512.png` - 512x512 pixels

## Creating Icons

### Windows .ico file
You can create a .ico file using ImageMagick:
```bash
convert icon_256.png -define icon:auto-resize=256,128,64,48,32,16 icon.ico
```

### macOS .icns file
You can create a .icns file using iconutil on macOS:
```bash
mkdir icon.iconset
cp icon_16.png icon.iconset/icon_16x16.png
cp icon_32.png icon.iconset/icon_16x16@2x.png
cp icon_32.png icon.iconset/icon_32x32.png
cp icon_64.png icon.iconset/icon_32x32@2x.png
cp icon_128.png icon.iconset/icon_128x128.png
cp icon_256.png icon.iconset/icon_128x128@2x.png
cp icon_256.png icon.iconset/icon_256x256.png
cp icon_512.png icon.iconset/icon_256x256@2x.png
cp icon_512.png icon.iconset/icon_512x512.png
iconutil -c icns icon.iconset
```