// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class PartList
 * @description
 * The `PartList` component renders a `.b-partlist` element to arrange Clip objects for playback.
 */

// == STYLE ==
Extra_css`
.b-partlist {
  display: flex;
  position: relative;
  .b-clipview {
    margin: 0 1px;
    width: calc(5 * var(--b-clipthumb-width));
    flex-shrink: 0; flex-grow: 0;
  }
}`;

// == COMPONENT ==
export function PartList (props)
{
  function dblclick (event)
  {
    if (props.track && typeof props.track.create_part === 'function')	// TODO: re-add Ase.Track.create_part()
      props.track.create_part (0);
  }

  return (
    <div class="b-partlist" data-f1="#part-list" onDblClick={dblclick}>
    </div>
  );
}
