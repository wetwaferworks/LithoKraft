# LithoKraft low-poly dropped items

Built 2026-11-17 for JD by CloudAtlas. **Show JD first. Nothing has been sent. Engine source and live packs were not touched.**

These are the floating item entities that appear on the ground when a component or block is thrown. Geometry is deliberately low-poly and reuses the placed shape: full blocks are cubes, official slabs are half-slabs, Trace logic plates reuse their 3/16 (or side-band) height, and a few components get the simplest silhouette that still reads as that part.

## Suggested drop motion (engine unchanged)

- Scale: **0.30** of a block (inside the 0.25–0.33 range).
- Hover: **0.08** block above the ground.
- Bob: amplitude **0.10** block, period **2.0s**.
- Spin: yaw **40** deg/s.

The engine does not yet have an item-drop renderer that reads these meshes (`.lthm` is a voxel format, not a low-poly mesh). Each item is exported as an engine-neutral JSON mesh plus OBJ and glTF for viewing.

## Counts by shape class

| Shape class | Count | Triangle budget | Notes |
|---|---:|---|---|
| `full_block` | 54 | 12 | 6 quads (12 tris). Watchor and Pushor are cubes; the eye and honeycomb are in the textures. |
| `special` | 20 | 12–48 (target) | 12–48 tris target (a few cages/pots may sit near the top of the range). |
| `component_slab` | 14 | 12 | 6 quads (12 tris). Height is at least 3/16 and grows to the opaque side-band so the art is not cropped. |
| `half_slab` | 3 | 12 | 6 quads (12 tris), height 8/16, side UVs cropped to the authored bottom half. |
| `foliage` | 16 | 8 | Two crossed planes, two-sided (8 tris). |
| **Total** | **107** | | |

## Judgment calls

- Official `Shape::Slab` blocks (silicon, stone, dirt) are half-slabs at 8/16. Trace logic plates are **not** forced to 8/16: JD said we can reuse the geometry, and the placed components are 3/16 plates (some with a taller housing). Height is at least 3/16 and grows to the opaque band of the side texture so that art is not cropped. If JD wants every plate drawn as an 8/16 half-slab, that is a one-line change.
- Watchor and Pushor stay full cubes (12 tris). The eye and honeycomb are in the approved concept-B face textures, which already read at drop scale.
- Swivelor is a cube plus an 8-sided equatorial gear belt so the ring silhouette still reads.
- Dialor is a component plate plus an 8-sided dial disc. The top is the approved v32 still with tonight's widened well.
- Via drops use the Via B lattice cage (straight single, tinted s64 so red and blue read). Side vias have no unique drop. Elbows have no drop.
- Red / Blue Trace are a **proposal**: a 1/16 chip with the KEEP-OLD straight-wire texture and a raised centre node, because the world wire is non-volumetric.
- Resin is the approved look-B pot. The engine string id `lithokraft:resin` is still proposed / OPEN. No resin block was designed.
- Switch and Button keep the component plate and add a simple lever box or raised cap.
- LEDs (torches) use a shaft + gem + chip, with the official `led_*` textures, on state.
- Ramps are a single wedge. Highway uses height 8/16 (one half of the paired 1:2 slope). Both reuse `aluminum_block` textures; resources.json has no dedicated ramp keys.
- Standing posts are a slim 4/16 column, shortened to 1.25 tall so they still read at 0.30 scale (the world post is 2 tall). Wall posts are a thin panel.
- Lithographor is a body box plus a lens column, simplified from the custom machine mesh.
- Capacitor Can is an 8-sided cylinder; Resonator Crystal is a 6-sided prism with a pointed cap.
- Foliage and Cross plants (except the LEDs) are two crossed planes.
- Readout tops use tonight's approved well fixes: Breakor option A (closed, ghost 88), Latchor off, Dialor v32, Fusor adder with the baked ghost 88. Other readout plates use their current idle / representative still (Repeator d16, Bitor AND, Memor idle, and so on).
- `lithokraft:redirector` is a live alias of `lithokraft:multiplexor`. The approved standalone Redirector is `lithokraft:routor`. No unique redirector id was invented.
- Concept-B / concept-C filenames are preferred when they exist in the approved component packs (`components-v2/<name>/pack_128`).

## Inventory

| Item | Engine id | Shape class | Tris | Textures used | Status |
|---|---|---|---:|---|---|
| Repeator | `lithokraft:repeator` | component_slab | 12 | top: `repeator_top_d16_base`; bottom: `repeator_side_base`; east: `repeator_side_base`; west: `repeator_side_base`; north: `repeator_front_base`; south: `repeator_back_base` | ready |
| Breakor | `lithokraft:fusor` | component_slab | 12 | top: `breakor_top_closed_base`; bottom: `breakor_side_closed_base`; east: `breakor_side_closed_base`; west: `breakor_side_closed_base`; north: `breakor_front_closed_base`; south… | ready |
| Fusor | `lithokraft:fusor_sum` | component_slab | 12 | top: `fusor_sum_top_base`; bottom: `fusor_sum_side_base`; east: `fusor_sum_side_base`; west: `fusor_sum_side_base`; north: `fusor_sum_front_base`; south: `fusor_sum_back_base` | ready |
| Dividor | `lithokraft:dividor` | component_slab | 12 | top: `dividor_top_base`; bottom: `dividor_side_base`; east: `dividor_side_base`; west: `dividor_side_base`; north: `dividor_front_base`; south: `dividor_back_base` | ready |
| Memor | `lithokraft:memor` | component_slab | 12 | top: `memor_top_idle_base`; bottom: `memor_side_idle_base`; east: `memor_side_idle_base`; west: `memor_side_idle_base`; north: `memor_front_idle_base`; south: `memor_back_idle_b… | ready |
| Thresholdor | `lithokraft:thresholdor` | component_slab | 12 | top: `thresholdor_top_compare_base`; bottom: `thresholdor_side_compare_base`; east: `thresholdor_side_compare_base`; west: `thresholdor_side_compare_base`; north: `thresholdor_f… | ready |
| Multiplexor | `lithokraft:multiplexor` | component_slab | 12 | top: `multiplexor_top_sel_red_base`; bottom: `multiplexor_side_sel_red_base`; east: `multiplexor_side_sel_red_base`; west: `multiplexor_side_sel_red_base`; north: `multiplexor_f… | ready |
| Routor | `lithokraft:routor` | component_slab | 12 | top: `routor_mode1_top_base`; bottom: `routor_mode1_side_base`; east: `routor_mode1_side_base`; west: `routor_mode1_side_base`; north: `routor_mode1_front_base`; south: `routor_… | ready |
| Pulsor | `lithokraft:pulsor` | component_slab | 12 | top: `pulsor_top_idle_base`; bottom: `pulsor_side_idle_base`; east: `pulsor_side_idle_base`; west: `pulsor_side_idle_base`; north: `pulsor_front_idle_base`; south: `pulsor_back_… | ready |
| Latchor | `lithokraft:fast_latch` | component_slab | 12 | top: `latchor_top_off_base`; bottom: `fast_latch_side_off_base`; east: `fast_latch_side_off_base`; west: `fast_latch_side_off_base`; north: `fast_latch_front_off_base`; south: `… | ready |
| Randor | `lithokraft:randor` | component_slab | 12 | top: `randor_top_idle_base`; bottom: `randor_side_idle_base`; east: `randor_side_idle_base`; west: `randor_side_idle_base`; north: `randor_front_idle_base`; south: `randor_back_… | ready |
| Catchor | `lithokraft:catchor` | component_slab | 12 | top: `catchor_top_idle_base`; bottom: `catchor_side_idle_base`; east: `catchor_side_idle_base`; west: `catchor_side_idle_base`; north: `catchor_front_idle_base`; south: `catchor… | ready |
| Keyboard | `lithokraft:keyboard` | component_slab | 12 | top: `keyboard_top_idle_base`; bottom: `keyboard_side_idle_base`; east: `keyboard_side_idle_base`; west: `keyboard_side_idle_base`; north: `keyboard_front_idle_base`; south: `ke… | ready |
| Switch | `lithokraft:switch` | special | 24 | top: `switch_top_off_base`; bottom: `switch_side_off_base`; east: `switch_side_off_base`; west: `switch_side_off_base`; north: `switch_front_off_base`; south: `switch_back_off_b… | ready |
| Button | `lithokraft:button` | special | 24 | top: `button_top_up_base`; bottom: `button_side_up_base`; east: `button_side_up_base`; west: `button_side_up_base`; north: `button_front_up_base`; south: `button_back_up_base` | ready |
| Dialor | `lithokraft:dialor` | special | 36 | top: `dialor_top_v32_base`; bottom: `dialor_side_base`; east: `dialor_side_base`; west: `dialor_side_base`; north: `dialor_front_base`; south: `dialor_back_base` | ready |
| Bitor | `lithokraft:bitor` | component_slab | 12 | top: `bitor_top_and_base`; bottom: `bitor_bottom_base`; east: `bitor_east_base`; west: `bitor_east_base`; north: `bitor_front_base`; south: `bitor_back_base` | ready |
| Watchor | `lithokraft:watchor` | full_block | 12 | top: `watchor_top_base`; bottom: `watchor_bottom_base`; east: `watchor_east_base`; west: `watchor_east_base`; north: `watchor_front_base`; south: `watchor_back_base` | ready |
| Pushor | `lithokraft:pushor` | full_block | 12 | top: `pushor_top_base`; bottom: `pushor_bottom_base`; east: `pushor_east_base`; west: `pushor_east_base`; north: `pushor_front_base`; south: `pushor_back_base` | ready |
| Swivelor | `lithokraft:swivelor` | special | 44 | top: `swivelor_top_d00_base`; bottom: `swivelor_bottom_base`; east: `swivelor_east_base`; west: `swivelor_east_base`; north: `swivelor_front_base`; south: `swivelor_back_base` | ready |
| Red Trace | `lithokraft:red_trace` | special | 36 | top: `red_trace`; bottom: `red_trace`; north: `red_trace`; south: `red_trace`; east: `red_trace`; west: `red_trace` | proposal |
| Blue Trace | `lithokraft:blue_trace` | special | 36 | top: `blue_trace`; bottom: `blue_trace`; north: `blue_trace`; south: `blue_trace`; east: `blue_trace`; west: `blue_trace` | proposal |
| Red Via | `lithokraft:red_via` | special | 64 | top: `via_top_single (red s64)`; bottom: `via_bottom_single (red s64)`; north: `via_side_single (red s64)`; south: `via_side_single (red s64)`; east: `via_side_single (red s64)`… | ready |
| Blue Via | `lithokraft:blue_via` | special | 64 | top: `via_top_single (blue s64)`; bottom: `via_bottom_single (blue s64)`; north: `via_side_single (blue s64)`; south: `via_side_single (blue s64)`; east: `via_side_single (blue … | ready |
| Vior | `lithokraft:vior` | full_block | 12 | top: `vior_run_end`; bottom: `vior_run_end`; east: `vior_run_side`; west: `vior_run_side`; north: `vior_run_side`; south: `vior_run_side` | ready |
| Red LED | `lithokraft:red_torch` | special | 36 | bottom: `led_chip`; top: `led_gem_red_off`; east: `led_shaft`; west: `led_shaft`; north: `led_shaft`; south: `led_shaft` | ready |
| Blue LED | `lithokraft:blue_torch` | special | 36 | bottom: `led_chip`; top: `led_gem_blue_off`; east: `led_shaft`; west: `led_shaft`; north: `led_shaft`; south: `led_shaft` | ready |
| Lamp | `lithokraft:lamp` | full_block | 12 | top: `lamp_on`; bottom: `lamp_on`; north: `lamp_on`; south: `lamp_on`; east: `lamp_on`; west: `lamp_on` | ready |
| lamp16 | `lithokraft:color_lamp_16` | full_block | 12 | top: `color_lamp_16`; bottom: `color_lamp_16`; north: `color_lamp_16`; south: `color_lamp_16`; east: `color_lamp_16`; west: `color_lamp_16` | ready |
| lamp32 | `lithokraft:color_lamp_32` | full_block | 12 | top: `color_lamp_32`; bottom: `color_lamp_32`; north: `color_lamp_32`; south: `color_lamp_32`; east: `color_lamp_32`; west: `color_lamp_32` | ready |
| lamp64 | `lithokraft:color_lamp_64` | full_block | 12 | top: `color_lamp_64`; bottom: `color_lamp_64`; north: `color_lamp_64`; south: `color_lamp_64`; east: `color_lamp_64`; west: `color_lamp_64` | ready |
| Wafer Silicon | `lithokraft:silicon` | full_block | 12 | top: `silicon`; bottom: `silicon`; north: `silicon`; south: `silicon`; east: `silicon`; west: `silicon` | ready |
| Silicon Slab | `lithokraft:silicon_slab` | half_slab | 12 | bottom: `silicon_slab_top`; top: `silicon_slab_top`; east: `silicon_slab_side`; west: `silicon_slab_side`; north: `silicon_slab_side`; south: `silicon_slab_side` | ready |
| Stone | `lithokraft:stone` | full_block | 12 | top: `stone`; bottom: `stone`; north: `stone`; south: `stone`; east: `stone`; west: `stone` | ready |
| Stone Slab | `lithokraft:stone_slab` | half_slab | 12 | bottom: `stone_slab_top`; top: `stone_slab_top`; east: `stone_slab_side`; west: `stone_slab_side`; north: `stone_slab_side`; south: `stone_slab_side` | ready |
| Dirt | `lithokraft:dirt` | full_block | 12 | top: `dirt`; bottom: `dirt`; north: `dirt`; south: `dirt`; east: `dirt`; west: `dirt` | ready |
| Dirt Slab | `lithokraft:dirt_slab` | half_slab | 12 | bottom: `dirt_slab_top`; top: `dirt_slab_top`; east: `dirt_slab_side`; west: `dirt_slab_side`; north: `dirt_slab_side`; south: `dirt_slab_side` | ready |
| Grass | `lithokraft:grass` | full_block | 12 | bottom: `dirt`; top: `grass_top`; east: `grass_side`; west: `grass_side`; north: `grass_side`; south: `grass_side` | ready |
| Glass | `lithokraft:glass` | full_block | 12 | top: `glass`; bottom: `glass`; north: `glass`; south: `glass`; east: `glass`; west: `glass` | ready |
| Frosted Glass | `lithokraft:frosted_glass` | full_block | 12 | top: `frosted_glass`; bottom: `frosted_glass`; north: `frosted_glass`; south: `frosted_glass`; east: `frosted_glass`; west: `frosted_glass` | ready |
| Ceramic | `lithokraft:ceramic` | full_block | 12 | top: `ceramic`; bottom: `ceramic`; north: `ceramic`; south: `ceramic`; east: `ceramic`; west: `ceramic` | ready |
| Copper Block | `lithokraft:copper_block` | full_block | 12 | top: `copper_block`; bottom: `copper_block`; north: `copper_block`; south: `copper_block`; east: `copper_block`; west: `copper_block` | ready |
| Aluminum Block | `lithokraft:aluminum_block` | full_block | 12 | top: `aluminum_block`; bottom: `aluminum_block`; north: `aluminum_block`; south: `aluminum_block`; east: `aluminum_block`; west: `aluminum_block` | ready |
| Gravity Block | `lithokraft:gravity_block` | full_block | 12 | top: `gravity_block`; bottom: `gravity_block`; north: `gravity_block`; south: `gravity_block`; east: `gravity_block`; west: `gravity_block` | ready |
| Substrate | `lithokraft:substrate` | full_block | 12 | top: `substrate`; bottom: `substrate`; north: `substrate`; south: `substrate`; east: `substrate`; west: `substrate` | ready |
| Insulatnor | `lithokraft:insulatnor` | full_block | 12 | top: `insulatnor`; bottom: `insulatnor`; north: `insulatnor`; south: `insulatnor`; east: `insulatnor`; west: `insulatnor` | ready |
| Insulator | `lithokraft:insulator` | full_block | 12 | top: `insulator`; bottom: `insulator`; north: `insulator`; south: `insulator`; east: `insulator`; west: `insulator` | ready |
| Insulatxor | `lithokraft:insulatxor` | full_block | 12 | top: `insulatxor`; bottom: `insulatxor`; north: `insulatxor`; south: `insulatxor`; east: `insulatxor`; west: `insulatxor` | ready |
| Structure Block | `lithokraft:structure_block` | full_block | 12 | top: `structure_block`; bottom: `structure_block`; north: `structure_block`; south: `structure_block`; east: `structure_block`; west: `structure_block` | ready |
| Lithographor | `lithokraft:lithographor` | special | 24 | top: `lithographor_top`; bottom: `lithographor_side`; east: `lithographor_side`; west: `lithographor_side`; north: `lithographor_front`; south: `lithographor_back` | ready |
| Etchor | `lithokraft:etchor` | full_block | 12 | top: `etchor_top_blue_in`; bottom: `etchor_bottom_red_in`; east: `etchor_side_laser_port`; west: `etchor_side_laser_port`; north: `etchor_front_chip_out`; south: `etchor_back_wa… | ready |
| Certificate Table | `lithokraft:certificate_table` | full_block | 12 | top: `certificate_table_top`; east: `certificate_table_side`; west: `certificate_table_side`; north: `certificate_table_side`; south: `certificate_table_side`; bottom: `certific… | ready |
| Emitor | `lithokraft:emitor` | full_block | 12 | top: `emitor_top`; bottom: `emitor_top`; east: `emitor_side`; west: `emitor_side`; north: `emitor_aperture`; south: `emitor_back_trace_in` | ready |
| Laysor | `lithokraft:laysor` | full_block | 12 | top: `laysor_crystal_top`; bottom: `laysor_crystal_top`; east: `laysor_crystal_side`; west: `laysor_crystal_side`; north: `laysor_crystal_end`; south: `laysor_crystal_end` | ready |
| Selector | `lithokraft:selector` | full_block | 12 | top: `selector_top`; bottom: `selector_bottom_out`; east: `selector_side`; west: `selector_side`; north: `selector_side`; south: `selector_side_bus` | ready |
| Connector | `lithokraft:connector` | full_block | 12 | top: `connector_top`; bottom: `connector_bottom`; east: `connector_side`; west: `connector_side`; north: `connector_side`; south: `connector_side` | ready |
| Input Buffer | `lithokraft:input_buffer` | full_block | 12 | top: `buffer_top_in`; east: `buffer_side`; west: `buffer_side`; north: `buffer_front_out`; south: `buffer_side`; bottom: `buffer_top_in` | ready |
| Fabor | `lithokraft:fabor` | full_block | 12 | top: `fabor_top`; east: `fabor_side`; west: `fabor_side`; north: `fabor_front`; south: `fabor_side`; bottom: `fabor_top` | ready |
| AutoFabor | `lithokraft:autofabor` | full_block | 12 | top: `autofabor_top`; east: `autofabor_side`; west: `autofabor_side`; north: `autofabor_front`; south: `autofabor_side`; bottom: `autofabor_top` | ready |
| Chipor | `lithokraft:chipor` | full_block | 12 | top: `chipor_top`; bottom: `chipor_side`; east: `chipor_side`; west: `chipor_side`; north: `chipor_front`; south: `chipor_back` | ready |
| Chipor Viewport | `lithokraft:chipor_viewport` | full_block | 12 | top: `chipor_viewport_top`; bottom: `chipor_viewport_side`; east: `chipor_viewport_side`; west: `chipor_viewport_side`; north: `chipor_viewport_front`; south: `chipor_viewport_b… | ready |
| Signal Transmittor | `lithokraft:signal_transmittor` | full_block | 12 | top: `signal_transmittor_top`; bottom: `signal_transmittor_side`; east: `signal_transmittor_side`; west: `signal_transmittor_side`; north: `signal_transmittor_front`; south: `si… | ready |
| Signal Receivor | `lithokraft:signal_receivor` | full_block | 12 | top: `signal_receivor_top`; bottom: `signal_receivor_side`; east: `signal_receivor_side`; west: `signal_receivor_side`; north: `signal_receivor_front`; south: `signal_receivor_b… | ready |
| Dish | `lithokraft:dish` | full_block | 12 | top: `dish_top`; bottom: `dish_side`; east: `dish_side`; west: `dish_side`; north: `dish_front`; south: `dish_back` | ready |
| Overclockor | `lithokraft:overclockor` | full_block | 12 | top: `overclockor_top`; bottom: `overclockor_side`; east: `overclockor_side`; west: `overclockor_side`; north: `overclockor_front`; south: `overclockor_back` | ready |
| Portor | `lithokraft:portor` | full_block | 12 | top: `portor_top`; bottom: `portor_side`; east: `portor_side`; west: `portor_side`; north: `portor_front`; south: `portor_back` | ready |
| Trace Trunk | `lithokraft:trace_trunk` | full_block | 12 | top: `trace_bark`; bottom: `trace_bark`; north: `trace_bark`; south: `trace_bark`; east: `trace_bark`; west: `trace_bark` | ready |
| Trace Branch | `lithokraft:trace_branch` | full_block | 12 | top: `trace_canopy`; bottom: `trace_canopy`; north: `trace_canopy`; south: `trace_canopy`; east: `trace_canopy`; west: `trace_canopy` | ready |
| Red Trace Bulb | `lithokraft:trace_tip_red` | full_block | 12 | top: `red_trace_bulb_ghost`; bottom: `red_trace_bulb_ghost`; north: `red_trace_bulb_ghost`; south: `red_trace_bulb_ghost`; east: `red_trace_bulb_ghost`; west: `red_trace_bulb_gh… | ready |
| Blue Trace Bulb | `lithokraft:trace_tip_blue` | full_block | 12 | top: `blue_trace_bulb_ghost`; bottom: `blue_trace_bulb_ghost`; north: `blue_trace_bulb_ghost`; south: `blue_trace_bulb_ghost`; east: `blue_trace_bulb_ghost`; west: `blue_trace_b… | ready |
| Capacitor Can | `lithokraft:capacitor_can` | special | 32 | top: `capacitor_side`; bottom: `capacitor_side`; north: `capacitor_side`; south: `capacitor_side`; east: `capacitor_side`; west: `capacitor_side` | ready |
| Resonator Crystal | `lithokraft:resonator_crystal` | special | 24 | top: `quartz_spire`; bottom: `quartz_spire`; north: `quartz_spire`; south: `quartz_spire`; east: `quartz_spire`; west: `quartz_spire` | ready |
| Dead Silicon | `lithokraft:dead_silicon` | full_block | 12 | top: `wafer_ground_dead`; bottom: `wafer_ground_dead`; north: `wafer_ground_dead`; south: `wafer_ground_dead`; east: `wafer_ground_dead`; west: `wafer_ground_dead` | ready |
| Fossil Trace | `lithokraft:fossil_trace` | full_block | 12 | top: `wafer_ground_trace`; bottom: `wafer_ground_trace`; north: `wafer_ground_trace`; south: `wafer_ground_trace`; east: `wafer_ground_trace`; west: `wafer_ground_trace` | ready |
| Broken Via | `lithokraft:broken_via` | full_block | 12 | top: `wafer_ground_dead`; bottom: `wafer_ground_dead`; north: `wafer_ground_dead`; south: `wafer_ground_dead`; east: `wafer_ground_dead`; west: `wafer_ground_dead` | ready |
| Scribe Silicon | `lithokraft:scribe_silicon` | full_block | 12 | top: `wafer_ground_dicing`; bottom: `wafer_ground_dicing`; north: `wafer_ground_dicing`; south: `wafer_ground_dicing`; east: `wafer_ground_dicing`; west: `wafer_ground_dicing` | ready |
| Dense Silicon | `lithokraft:dense_silicon` | full_block | 12 | top: `wafer_ground_capacitor`; bottom: `wafer_ground_capacitor`; north: `wafer_ground_capacitor`; south: `wafer_ground_capacitor`; east: `wafer_ground_capacitor`; west: `wafer_g… | ready |
| Flecked Silicon | `lithokraft:flecked_silicon` | full_block | 12 | top: `wafer_ground_resonator`; bottom: `wafer_ground_resonator`; north: `wafer_ground_resonator`; south: `wafer_ground_resonator`; east: `wafer_ground_resonator`; west: `wafer_g… | ready |
| Dead Memory Cell | `lithokraft:dead_memory_cell` | full_block | 12 | top: `memor_top_idle_base`; bottom: `memor_side_idle_base`; east: `memor_side_idle_base`; west: `memor_side_idle_base`; north: `memor_front_idle_base`; south: `memor_back_idle_b… | ready |
| Tilled Silicon | `lithokraft:tilled_silicon` | full_block | 12 | top: `silica_tilled`; bottom: `silica_tilled`; north: `silica_tilled`; south: `silica_tilled`; east: `silica_tilled`; west: `silica_tilled` | ready |
| Aluminum Ramp | `lithokraft:aluminum_ramp` | special | 8 | top: `aluminum_block`; bottom: `aluminum_block`; north: `aluminum_block`; south: `aluminum_block`; east: `aluminum_block`; west: `aluminum_block` | ready |
| Highway Ramp | `lithokraft:highway_ramp` | special | 8 | top: `aluminum_block`; bottom: `aluminum_block`; north: `aluminum_block`; south: `aluminum_block`; east: `aluminum_block`; west: `aluminum_block` | ready |
| Virtual Post | `lithokraft:virtual_post` | special | 12 | top: `post_wood_virtual`; east: `post_wood_virtual`; west: `post_wood_virtual`; north: `post_wood_virtual`; south: `post_wood_virtual`; bottom: `post_wood_virtual` | ready |
| Organic Post | `lithokraft:organic_post` | special | 12 | top: `post_wood_organic`; east: `post_wood_organic`; west: `post_wood_organic`; north: `post_wood_organic`; south: `post_wood_organic`; bottom: `post_wood_organic` | ready |
| Virtual Wall Post | `lithokraft:virtual_wall_post` | special | 12 | top: `post_wood_virtual`; east: `post_wood_virtual`; west: `post_wood_virtual`; north: `post_wood_virtual`; south: `post_wood_virtual`; bottom: `post_wood_virtual` | ready |
| Organic Wall Post | `lithokraft:organic_wall_post` | special | 12 | top: `post_wood_organic`; east: `post_wood_organic`; west: `post_wood_organic`; north: `post_wood_organic`; south: `post_wood_organic`; bottom: `post_wood_organic` | ready |
| Crystal Leaves | `lithokraft:crystal_leaves_a` | full_block | 12 | top: `crystal_leaves_a`; bottom: `crystal_leaves_a`; north: `crystal_leaves_a`; south: `crystal_leaves_a`; east: `crystal_leaves_a`; west: `crystal_leaves_a` | ready |
| Crystal Leaves (Sparse) | `lithokraft:crystal_leaves_b` | full_block | 12 | top: `crystal_leaves_b`; bottom: `crystal_leaves_b`; north: `crystal_leaves_b`; south: `crystal_leaves_b`; east: `crystal_leaves_b`; west: `crystal_leaves_b` | ready |
| Red Crystal Leaves | `lithokraft:crystal_leaves_reddom` | full_block | 12 | top: `crystal_leaves_reddom`; bottom: `crystal_leaves_reddom`; north: `crystal_leaves_reddom`; south: `crystal_leaves_reddom`; east: `crystal_leaves_reddom`; west: `crystal_leav… | ready |
| Blue Crystal Leaves | `lithokraft:crystal_leaves_bluedom` | full_block | 12 | top: `crystal_leaves_bluedom`; bottom: `crystal_leaves_bluedom`; north: `crystal_leaves_bluedom`; south: `crystal_leaves_bluedom`; east: `crystal_leaves_bluedom`; west: `crystal… | ready |
| Silica Grass | `lithokraft:silica_grass` | foliage | 8 | top: `silica_grass`; bottom: `silica_grass`; north: `silica_grass`; south: `silica_grass`; east: `silica_grass`; west: `silica_grass` | ready |
| Grass Tuft | `lithokraft:procgen_grass_tuft` | foliage | 8 | top: `procgen_grass_tuft`; bottom: `procgen_grass_tuft`; north: `procgen_grass_tuft`; south: `procgen_grass_tuft`; east: `procgen_grass_tuft`; west: `procgen_grass_tuft` | ready |
| Fern | `lithokraft:procgen_fern` | foliage | 8 | top: `procgen_fern`; bottom: `procgen_fern`; north: `procgen_fern`; south: `procgen_fern`; east: `procgen_fern`; west: `procgen_fern` | ready |
| Dry Scrub | `lithokraft:procgen_dry_scrub` | foliage | 8 | top: `procgen_dry_scrub`; bottom: `procgen_dry_scrub`; north: `procgen_dry_scrub`; south: `procgen_dry_scrub`; east: `procgen_dry_scrub`; west: `procgen_dry_scrub` | ready |
| Formula Flower | `lithokraft:procgen_formula_flower` | foliage | 8 | top: `procgen_formula_flower`; bottom: `procgen_formula_flower`; north: `procgen_formula_flower`; south: `procgen_formula_flower`; east: `procgen_formula_flower`; west: `procgen… | ready |
| Formula Star | `lithokraft:procgen_formula_star` | foliage | 8 | top: `procgen_formula_star`; bottom: `procgen_formula_star`; north: `procgen_formula_star`; south: `procgen_formula_star`; east: `procgen_formula_star`; west: `procgen_formula_s… | ready |
| Formula Leaf | `lithokraft:procgen_formula_leaf` | foliage | 8 | top: `procgen_formula_leaf`; bottom: `procgen_formula_leaf`; north: `procgen_formula_leaf`; south: `procgen_formula_leaf`; east: `procgen_formula_leaf`; west: `procgen_formula_l… | ready |
| Formula Blob | `lithokraft:procgen_formula_blob` | foliage | 8 | top: `procgen_formula_blob`; bottom: `procgen_formula_blob`; north: `procgen_formula_blob`; south: `procgen_formula_blob`; east: `procgen_formula_blob`; west: `procgen_formula_b… | ready |
| Formula Shrub 311 | `lithokraft:formula_shrub_311` | foliage | 8 | top: `formula_shrub_311`; bottom: `formula_shrub_311`; north: `formula_shrub_311`; south: `formula_shrub_311`; east: `formula_shrub_311`; west: `formula_shrub_311` | ready |
| Formula Shrub 314 | `lithokraft:formula_shrub_314` | foliage | 8 | top: `formula_shrub_314`; bottom: `formula_shrub_314`; north: `formula_shrub_314`; south: `formula_shrub_314`; east: `formula_shrub_314`; west: `formula_shrub_314` | ready |
| Cattails | `lithokraft:foliage_cattail` | foliage | 8 | top: `foliage_m33`; bottom: `foliage_m33`; north: `foliage_m33`; south: `foliage_m33`; east: `foliage_m33`; west: `foliage_m33` | ready |
| Berry Bush | `lithokraft:foliage_bush` | foliage | 8 | top: `foliage_m34`; bottom: `foliage_m34`; north: `foliage_m34`; south: `foliage_m34`; east: `foliage_m34`; west: `foliage_m34` | ready |
| Fern Clump | `lithokraft:foliage_fern` | foliage | 8 | top: `foliage_m35`; bottom: `foliage_m35`; north: `foliage_m35`; south: `foliage_m35`; east: `foliage_m35`; west: `foliage_m35` | ready |
| Reed Tuft | `lithokraft:foliage_reeds` | foliage | 8 | top: `foliage_m36`; bottom: `foliage_m36`; north: `foliage_m36`; south: `foliage_m36`; east: `foliage_m36`; west: `foliage_m36` | ready |
| Crystal Shrub | `lithokraft:foliage_crystal_shrub` | foliage | 8 | top: `foliage_m37`; bottom: `foliage_m37`; north: `foliage_m37`; south: `foliage_m37`; east: `foliage_m37`; west: `foliage_m37` | ready |
| Routing Moss | `lithokraft:foliage_moss` | foliage | 8 | top: `foliage_m34`; bottom: `foliage_m34`; north: `foliage_m34`; south: `foliage_m34`; east: `foliage_m34`; west: `foliage_m34` | ready |
| Resin | `lithokraft:resin` | special | 40 | top: `resin_item_B_top.png`; bottom: `resin_item_B_bottom.png`; north: `resin_item_B_front.png`; south: `resin_item_B_back.png`; east: `resin_item_B_east.png`; west: `resin_item… | ready |

## Not given their own drop

- `lithokraft:red_via_x` / `red_via_z` / `blue_via_x` / `blue_via_z` — side vias drop as their vertical colour (`viaBase` in `blocks.h`).
- Via elbows — auto-added geometry, not a separately placed block.
- `lithokraft:tone_block` and `lithokraft:timer_buffer` — discarded by JD on 2026-11-04.
- `lithokraft:redirector` — live alias of `lithokraft:multiplexor`. The approved standalone Redirector is `lithokraft:routor`. No unique id was invented.
- Mortor — still PENDING-REVIEW, no engine id.

## Rebuild

```bash
python3 /workspace/components-v2/item-drops-2026-11-17/build_item_drops.py
```

The script refuses to overwrite any existing file in this folder (including models, textures, previews, and this README). To rebuild, move or version the folder first.

## Layout

- `models/<id>.json` — engine-neutral mesh (positions, UVs, faces, texture refs, drop-scale suggestion).
- `models/<id>.obj` + `.mtl` + `.bin` / `.gltf` — viewable exports.
- `textures/` — copied 128 px face textures (approved well-fix tops used where they apply).
- `previews/<id>.png` — 512 px 3/4-view still, floating pose, suggested scale, on a ground disc.
- `overview.png` — 3840×2160 sheet titled LITHOKRAFT.
- `overview-preview.png` — 1600 px wide preview.

