// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/// Wait for a single frame (~17ms at 60Hz) for simple roundtrips
export async function wait_frame (): Promise<void>
{
  await new Promise (r => setTimeout (r, 17));
}
