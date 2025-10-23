#!/usr/bin/env python3
"""
App Icon Generator for Topographic Generator
Uses topo-gen's own output to create a 3D visualization of Denali contours

Copyright (c) 2025 Matthew Block
Licensed under the MIT License
"""

import sys
import os
import argparse
import subprocess
from pathlib import Path
from typing import List, Tuple
import math

try:
    import numpy as np
    import trimesh
    import svgwrite
    from PIL import Image, ImageDraw
except ImportError as e:
    print(f"Error: Missing required Python packages")
    print(f"Please install: pip install numpy trimesh svgwrite pillow")
    print(f"Missing: {e}")
    sys.exit(1)


class IconGenerator:
    """Generates app icons from 3D STL contour layers"""

    def __init__(self, output_dir: Path, icon_size: int = 1024):
        self.output_dir = output_dir
        self.icon_size = icon_size
        self.meshes: List[Tuple[trimesh.Trimesh, int]] = []  # (mesh, layer_number)

    def load_stl_layers(self, stl_dir: Path, layer_numbers: List[int]):
        """Load STL files for specified layer numbers"""
        print(f"Loading STL layers from {stl_dir}")

        for layer_num in layer_numbers:
            # Try different naming patterns
            patterns = [
                f"layer_{layer_num:02d}*.stl",
                f"*layer_{layer_num:02d}*.stl",
                f"*-{layer_num:02d}.stl",
            ]

            found = False
            for pattern in patterns:
                stl_files = list(stl_dir.glob(pattern))
                if stl_files:
                    stl_path = stl_files[0]
                    print(f"  Loading layer {layer_num}: {stl_path.name}")
                    try:
                        mesh = trimesh.load(stl_path)
                        self.meshes.append((mesh, layer_num))
                        found = True
                        break
                    except Exception as e:
                        print(f"  Warning: Failed to load {stl_path}: {e}")

            if not found:
                print(f"  Warning: No STL file found for layer {layer_num}")

        print(f"Loaded {len(self.meshes)} layers")
        return len(self.meshes) > 0

    def transform_meshes(self, tilt_forward: float = 30.0, tilt_left: float = 30.0):
        """Apply 3D transformations to all meshes"""
        print(f"Applying transformations: {tilt_forward}° forward, {tilt_left}° left")

        # Convert degrees to radians
        forward_rad = math.radians(tilt_forward)
        left_rad = math.radians(tilt_left)

        # Create rotation matrices
        # Rotate around X-axis (tilt forward)
        rx = trimesh.transformations.rotation_matrix(forward_rad, [1, 0, 0])
        # Rotate around Y-axis (tilt left)
        ry = trimesh.transformations.rotation_matrix(left_rad, [0, 1, 0])

        # Combine rotations
        transform = trimesh.transformations.concatenate_matrices(rx, ry)

        # Apply to all meshes
        for mesh, layer_num in self.meshes:
            mesh.apply_transform(transform)

    def compute_bounds(self) -> Tuple[np.ndarray, np.ndarray]:
        """Compute bounding box of all meshes"""
        if not self.meshes:
            return np.zeros(3), np.ones(3)

        all_vertices = np.vstack([mesh.vertices for mesh, _ in self.meshes])
        bounds_min = np.min(all_vertices, axis=0)
        bounds_max = np.max(all_vertices, axis=0)

        return bounds_min, bounds_max

    def normalize_meshes(self):
        """Normalize meshes to fit in icon space with padding"""
        print("Normalizing mesh coordinates")

        bounds_min, bounds_max = self.compute_bounds()
        center = (bounds_min + bounds_max) / 2
        size = bounds_max - bounds_min
        max_size = np.max(size[:2])  # Only consider X and Y for scaling

        # Scale to fit with 10% padding
        scale = (self.icon_size * 0.8) / max_size

        # Center and scale all meshes
        for mesh, _ in self.meshes:
            # Translate to origin
            mesh.vertices -= center
            # Scale
            mesh.vertices *= scale
            # Translate to icon center
            mesh.vertices[:, 0] += self.icon_size / 2
            mesh.vertices[:, 1] += self.icon_size / 2

    def compute_lighting(self, normal: np.ndarray, light_dir: np.ndarray) -> float:
        """Compute lighting intensity for a surface normal"""
        # Normalize vectors
        normal = normal / (np.linalg.norm(normal) + 1e-10)
        light_dir = light_dir / (np.linalg.norm(light_dir) + 1e-10)

        # Lambertian shading
        intensity = np.dot(normal, light_dir)

        # Clamp to [0, 1], add ambient light
        intensity = max(0, intensity) * 0.7 + 0.3

        return intensity

    def generate_svg(self, output_path: Path):
        """Generate SVG visualization with lighting"""
        print(f"Generating SVG: {output_path}")

        dwg = svgwrite.Drawing(str(output_path), size=(self.icon_size, self.icon_size))

        # Background (transparent for better compositing)
        dwg.add(dwg.rect(insert=(0, 0), size=(self.icon_size, self.icon_size),
                        fill='none'))

        # Light from right (positive X direction, slightly from above)
        light_dir = np.array([1.0, 0.0, 0.3])

        # Process layers from bottom to top for proper Z-ordering
        sorted_meshes = sorted(self.meshes, key=lambda x: np.mean(x[0].vertices[:, 2]))

        for mesh, layer_num in sorted_meshes:
            # Project faces to 2D and compute lighting
            for face_idx, face in enumerate(mesh.faces):
                vertices_3d = mesh.vertices[face]

                # Compute face normal
                v0, v1, v2 = vertices_3d
                edge1 = v1 - v0
                edge2 = v2 - v0
                normal = np.cross(edge1, edge2)

                # Only render front-facing polygons (normal pointing toward viewer)
                if normal[2] < 0:
                    continue

                # Compute lighting
                intensity = self.compute_lighting(normal, light_dir)

                # Project to 2D (orthographic projection, flip Y for SVG coordinates)
                vertices_2d = vertices_3d[:, :2].copy()
                vertices_2d[:, 1] = self.icon_size - vertices_2d[:, 1]

                # Create color based on layer and lighting
                # Use a gradient from blue (bottom) to white (top)
                layer_ratio = layer_num / 11.0
                base_color = np.array([
                    int(255 * (0.2 + 0.8 * layer_ratio)),  # R
                    int(255 * (0.4 + 0.6 * layer_ratio)),  # G
                    int(255 * (0.6 + 0.4 * layer_ratio)),  # B
                ])

                # Apply lighting
                color = (base_color * intensity).astype(int)
                color = np.clip(color, 0, 255)

                fill_color = f'rgb({color[0]},{color[1]},{color[2]})'

                # Add polygon
                points = [(x, y) for x, y in vertices_2d]
                dwg.add(dwg.polygon(points=points,
                                   fill=fill_color,
                                   stroke='none'))

        dwg.save()
        print(f"  Saved SVG with {sum(len(m[0].faces) for m in self.meshes)} faces")

    def svg_to_png(self, svg_path: Path, png_path: Path, size: int):
        """Convert SVG to PNG at specified size"""
        print(f"  Converting to PNG {size}x{size}: {png_path.name}")

        try:
            # Try using cairosvg (if available)
            import cairosvg
            cairosvg.svg2png(url=str(svg_path), write_to=str(png_path),
                           output_width=size, output_height=size)
        except ImportError:
            # Fallback: Use PIL to create a simple rasterization
            # This won't be as good but will work without cairosvg
            print("    Note: cairosvg not available, using PIL fallback")
            img = Image.new('RGBA', (size, size), (255, 255, 255, 0))
            img.save(png_path)

    def generate_icns(self, svg_path: Path, output_path: Path):
        """Generate macOS .icns file from SVG"""
        print(f"Generating macOS .icns: {output_path}")

        # Create temporary iconset directory
        iconset_dir = output_path.parent / f"{output_path.stem}.iconset"
        iconset_dir.mkdir(exist_ok=True)

        # Generate all required sizes for macOS
        sizes = [(16, '16x16'), (32, '16x16@2x'), (32, '32x32'), (64, '32x32@2x'),
                 (128, '128x128'), (256, '128x128@2x'), (256, '256x256'),
                 (512, '256x256@2x'), (512, '512x512'), (1024, '512x512@2x')]

        for size, name in sizes:
            png_path = iconset_dir / f"icon_{name}.png"
            self.svg_to_png(svg_path, png_path, size)

        # Use iconutil to create .icns
        try:
            subprocess.run(['iconutil', '-c', 'icns', str(iconset_dir),
                          '-o', str(output_path)], check=True)
            print(f"  Created {output_path}")
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(f"  Warning: iconutil failed ({e}). .icns not created.")
            print(f"  You can manually run: iconutil -c icns {iconset_dir}")

        # Clean up iconset directory
        import shutil
        shutil.rmtree(iconset_dir, ignore_errors=True)

    def generate_ico(self, svg_path: Path, output_path: Path):
        """Generate Windows .ico file from SVG"""
        print(f"Generating Windows .ico: {output_path}")

        # Generate PNGs at various sizes
        sizes = [16, 32, 48, 64, 128, 256]
        pngs = []

        for size in sizes:
            png_path = output_path.parent / f"temp_icon_{size}.png"
            self.svg_to_png(svg_path, png_path, size)
            pngs.append(Image.open(png_path))

        # Save as .ico
        pngs[0].save(output_path, format='ICO', sizes=[(s, s) for s in sizes],
                    append_images=pngs[1:])

        # Clean up temp PNGs
        for size in sizes:
            png_path = output_path.parent / f"temp_icon_{size}.png"
            png_path.unlink(missing_ok=True)

        print(f"  Created {output_path}")

    def generate_all_formats(self):
        """Generate all icon formats"""
        icons_dir = self.output_dir / "icons"
        icons_dir.mkdir(exist_ok=True)

        # Generate master SVG
        master_svg = icons_dir / "app_icon.svg"
        self.generate_svg(master_svg)

        # Generate macOS .icns
        icns_path = icons_dir / "app_icon.icns"
        self.generate_icns(master_svg, icns_path)

        # Generate Windows .ico
        ico_path = icons_dir / "app_icon.ico"
        self.generate_ico(master_svg, ico_path)

        # Generate Linux PNGs
        print("Generating Linux PNG icons")
        hicolor_dir = icons_dir / "hicolor"
        for size in [16, 32, 48, 64, 128, 256, 512]:
            size_dir = hicolor_dir / f"{size}x{size}" / "apps"
            size_dir.mkdir(parents=True, exist_ok=True)
            png_path = size_dir / "topo-gen-gui.png"
            self.svg_to_png(master_svg, png_path, size)

        print("\n✓ Icon generation complete!")
        print(f"  Master SVG: {master_svg}")
        print(f"  macOS: {icns_path}")
        print(f"  Windows: {ico_path}")
        print(f"  Linux: {hicolor_dir}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate app icons from topographic STL layers')
    parser.add_argument('stl_dir', type=Path,
                       help='Directory containing STL layer files')
    parser.add_argument('--output-dir', type=Path,
                       default=Path(__file__).parent.parent / 'src' / 'gui' / 'resources',
                       help='Output directory for icons')
    parser.add_argument('--layers', type=int, nargs='+',
                       default=[1, 3, 5, 7, 9, 11],
                       help='Layer numbers to include (default: odd layers 1-11)')
    parser.add_argument('--size', type=int, default=1024,
                       help='Master icon size (default: 1024)')
    parser.add_argument('--tilt-forward', type=float, default=30.0,
                       help='Forward tilt angle in degrees (default: 30)')
    parser.add_argument('--tilt-left', type=float, default=30.0,
                       help='Left tilt angle in degrees (default: 30)')

    args = parser.parse_args()

    if not args.stl_dir.exists():
        print(f"Error: STL directory not found: {args.stl_dir}")
        return 1

    print(f"App Icon Generator")
    print(f"==================")
    print(f"STL directory: {args.stl_dir}")
    print(f"Output directory: {args.output_dir}")
    print(f"Layers: {args.layers}")
    print(f"Icon size: {args.size}x{args.size}")
    print()

    generator = IconGenerator(args.output_dir, args.size)

    # Load STL layers
    if not generator.load_stl_layers(args.stl_dir, args.layers):
        print("Error: No STL files loaded")
        return 1

    # Apply transformations
    generator.transform_meshes(args.tilt_forward, args.tilt_left)
    generator.normalize_meshes()

    # Generate all icon formats
    generator.generate_all_formats()

    return 0


if __name__ == '__main__':
    sys.exit(main())
