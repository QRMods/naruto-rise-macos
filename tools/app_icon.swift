// Shapes a picture as a macOS app icon (tools/build.py): a 1024x1024 PNG with
// the picture, cropped to a square, inside the standard rounded square of
// Apple's icon grid (824 points with a 100-point margin) over a soft shadow.
// macOS 26 shows an icon of that shape as it is; a full-bleed square one it
// shrinks into a grey frame.
//
// Usage: swift tools/app_icon.swift <picture> <icon.png>
import AppKit
import ImageIO

let arguments = CommandLine.arguments
guard arguments.count == 3 else {
  FileHandle.standardError.write("usage: swift app_icon.swift <picture> <icon.png>\n".data(using: .utf8)!)
  exit(2)
}
guard let picture = NSImage(contentsOfFile: arguments[1]),
      let image = picture.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
  FileHandle.standardError.write("app_icon: cannot read \(arguments[1])\n".data(using: .utf8)!)
  exit(1)
}

let canvas = 1024, margin: CGFloat = 100, side: CGFloat = 824, radius: CGFloat = 185.4
guard let space = CGColorSpace(name: CGColorSpace.sRGB),
      let context = CGContext(data: nil, width: canvas, height: canvas, bitsPerComponent: 8, bytesPerRow: 0,
                              space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { exit(1) }
let square = CGRect(x: margin, y: margin, width: side, height: side)
let shape = CGPath(roundedRect: square, cornerWidth: radius, cornerHeight: radius, transform: nil)

// The shadow, under the shape the picture then covers.
context.saveGState()
context.setShadow(offset: CGSize(width: 0, height: -10), blur: 24, color: CGColor(gray: 0, alpha: 0.35))
context.addPath(shape)
context.setFillColor(CGColor(gray: 0, alpha: 1))
context.fillPath()
context.restoreGState()

// The picture's centred square, clipped to the shape.
let width = CGFloat(image.width), height = CGFloat(image.height), crop = min(width, height)
let centred = image.cropping(to: CGRect(x: (width - crop) / 2, y: (height - crop) / 2, width: crop, height: crop)) ?? image
context.addPath(shape)
context.clip()
context.interpolationQuality = .high
context.draw(centred, in: square)

guard let icon = context.makeImage(),
      let output = CGImageDestinationCreateWithURL(URL(fileURLWithPath: arguments[2]) as CFURL,
                                                   "public.png" as CFString, 1, nil) else { exit(1) }
CGImageDestinationAddImage(output, icon, nil)
exit(CGImageDestinationFinalize(output) ? 0 : 1)
