# Hanako reference

- `hanako-input.png`: AI-generated black-and-white fan-art portrait of Hanako Ikezawa from *Katawa Shoujo*, generated with the built-in image generation tool for this project. Not official game artwork.
- `hanako-output.gif`: processed from that input by pikupiku, at 640 × 768 pixels, strength 2, five drawings, and 6 fps. GIF timing is rounded to centiseconds.

```sh
./pikupiku docs/reference/hanako-input.png hanako.gif --width 640 --strength 2 --frames 5 --fps 6
```

All remaining rendering options used their defaults. The character belongs to the respective rights holders; this project's MIT software license does not grant rights in the underlying character.

## Generation prompt

Use case: illustration-story. Create a black-and-white chest-up portrait of Hanako Ikezawa from Katawa Shoujo, recognizable as the fictional character: young woman with long dark hair partly covering the left side of her face, visible eye, gentle reserved expression, modest high-collared school uniform. Manga-inspired hand-drawn monochrome ink and graphite portrait, varied fine linework, soft cross-hatched gray shadows, white paper background, centered bust portrait, no color, no lettering, no watermark, no other characters.
