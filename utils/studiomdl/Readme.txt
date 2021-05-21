Half-Life Studiomodel Compiler by BrianOblivion/DoomMusic
Version 1.02

This is a modified version of the studiomdl from Valve's github. This
version is meant to fix the annoying texture shifting errors that occur
after compiling a model.

To do that, it does the following:
 - It applies floating point value rounding after the texture coordinates
   have been multiplied with the texture resolutions. This is done because
   Half-Life's MDL files store texcoords as integers.
 - It gets rid of texture resizing if the textures are not power of 2, and
   instead seeks the nearest larger power of 2 resolution, and places the
   original texture into the larger canvas. This is done because resizing
   8-bit textures will result in artifacts without a proper implementation,
   which studiomdl doesn't support.

Features(1.0):
 - This version fixes the issue with bones that only have attachments being
   removed and causing compile errors. Now, if your bone has no vertexes, but
   it still has attachments, it will not be optimized out.
 - I've removed texture coord cropping in the 0-1 range, so your texture coords
   can be freely set as you wish.
 - Currently the largest supported texture resolution for Half-Life is 512 in
   each dimension. If it's larger than this, HL will resize it back to 512 pixels,
   and it mighjt even crash.
   This can be overriden by setting the -o parameter, which is useful if you are
   running in an environment that supports textures larger than 512 pixels, like
   Xash and so.
 - $cliptotextures doesn't do anything anymore.
 - It supports Sven Coop's fullbright texture flag.

Features(1.01):
 - Added alpha, nomips, flatshade and chrome support for $texrendermode.
 - Added the -b parameter to specify a stretch-out border when padding textures
   that are not power of 2. Normally this has a value of 1. It should get rid of
   any artifacts with texcoords at the edges of textures.

Fixes(1.01):
 - An issue was fixed when loading in textures that are not multiples of 4 in width.
   The cause of this issue was that internally BMP stores them with the width as a
   multiple of 4, but studiomdl does not account for that when setting the bmp's width
   for itself.

Features(1.01)
 - Added the $protected qc command. This lets you specify bones which cannot
   be optimized out by the compiler even if they're not used by any vertices.
   Example:
   $protected "Bip01 Head"

Important notice:
Altough this tool removes UV capping outside the 0-1 range, you still neeed to
make sure you use power of 2 textures if you have UV coords outside of the 0-1
range, as the padding feature will mess that up.
In general you should never use non-power of 2 textures, since Half-Life already
resizes everything to power of 2 when loading models, which will cause loss of
quality in the process.

If you have any questions, please e-mail me at heroofdoom666@hotmail.com