<?php
$secret_key = "Z3nQ8yR5mC1vB7sK4pL2tG9hD6wF0aEJ";

function customEncrypt($data, $key) {
    $result = "";
    $dataLen = strlen($data);
    $keyLen = strlen($key);
    
    for ($i = 0; $i < $dataLen; $i++) {
        $c = ord($data[$i]);
        $k = ord($key[$i % $keyLen]);
        $encrypted = ($c + $k) & 0xFF;
        $result .= bin2hex(chr($encrypted));
    }
    
    return $result;
}

function customDecrypt($data, $key) {
    $result = "";
    $dataLen = strlen($data);
    $keyLen = strlen($key);
    
    for ($i = 0; $i < $dataLen; $i += 2) {
        if ($i + 1 < $dataLen) {
            $hex = substr($data, $i, 2);
            $value = hexdec($hex);
            $k = ord($key[($i / 2) % $keyLen]);
            $decrypted = ($value - $k) & 0xFF;
            $result .= chr($decrypted);
        }
    }
    
    return $result;
}

if (!isset($_POST['p2']) || !isset($_POST['p3']) || !isset($_POST['p4'])) {
    exit("Invalid request");
}

$key = customDecrypt($_POST['p2'], $secret_key);
$apiKey = customDecrypt($_POST['p3'], $secret_key);
$sumtin = customDecrypt($_POST['p4'], $secret_key);

$includePath = __DIR__ . "/keys.php";
if (is_file($includePath)) {
    include $includePath;
} else {
    exit(customEncrypt(json_encode([
        'status' => 'error',
        'message' => 'keys.php not found'
    ]), $secret_key));
}


if (strlen($key) < 5) {
    exit(customEncrypt(json_encode([
        'status' => 'error',
        'message' => 'Key too short'
    ]), $secret_key));
}

$pairsRaw = getKeyAndHWIDPairs($apiKey);
$pairs = json_decode($pairsRaw, true);

if (!is_array($pairs)) {
    exit(customEncrypt(json_encode([
        'status' => 'error',
        'message' => 'Failed to decode JSON or result is not an array',
        'debug' => var_export($pairsRaw, true)
    ]), $secret_key));
}

$valid = false;
foreach ($pairs as $index => $pair) {
    if (!is_array($pair)) {
        exit(customEncrypt(json_encode([
            'status' => 'error',
            'message' => "Invalid pair format at index $index",
            'debug' => var_export($pair, true)
        ]), $secret_key));
    }

    if (isset($pair['key']) && $pair['key'] === $key) {
        $valid = true;
        break;
    }
}

if (!$valid) {
    exit(customEncrypt(json_encode([
        'status' => 'error',
        'message' => 'Key not found in authorized list',
        'debug' => [
            'searched_key' => $key,
            'available_keys' => array_column($pairs, 'key')
        ]
    ]), $secret_key));
}

$offsets = [
    // version-36a2600cebf1487d (Theo dump 08/07/2026)
    'task_scheduler::pointer' => '0x8041ec8',
    'task_scheduler::job_start' => '0xc8',
    'task_scheduler::job_end' => '0xd0',
    'task_scheduler::job_name' => '0x18',
    'task_scheduler::job_stride' => '0x10',
    'task_scheduler::render_job_to_fake_datamodel' => '0x38',
    'task_scheduler::fake_datamodel_to_datamodel' => '0x1c8',
    'task_scheduler::render_job_to_renderview' => '0x1d0',
    'task_scheduler::max_fps' => '0xb0',
    'task_scheduler::target_fps' => '0x76a8730',

    'datamodel::datamodel_ptr0' => '0x84a9e98',
    'datamodel::datamodel_ptr1' => '0x1d0',
    'datamodel::place_id' => '0x190',

    'visualengine::visualengine_ptr' => '0x81d61c8',
    'visualengine::view_matrix' => '0x150',
    'visualengine::dimensions' => '0xab0',

    'renderview::force_flag_byte' => '0x150',
    'renderview::force_flag_bool' => '0x28d',

    'players::local_player' => '0x130',

    'player::display_name' => '0x138',
    'player::user_id' => '0x300',
    'player::team' => '0x2d8',
    'player::team_color' => '0x3ac',
    'player::character' => '0x298',

    'base_part::primitive' => '0x128',
    'base_part::material' => '0x22e',
    'base_part::transparency' => '0xd0',
    'base_part::color3' => '0x148',
    'base_part::size' => '0x1b8',
    'base_part::position' => '0xec',
    'base_part::primitive_properties' => '0xa0',
    'base_part::primitive_position' => '0x90',
    'base_part::validate' => '0x6',
    'base_part::cframe_rotation' => '0xc8',
    'base_part::assembly_linear_velocity' => '0xf8',
    'base_part::assembly_angular_velocity' => '0x104',
    'base_part::can_collide' => '0x1b6',
    'base_part::can_collide_mask' => '0x8',

    'humanoid::humanoid_state_id' => '0x20',
    'humanoid::move_direction' => '0x140',
    'humanoid::floor_material' => '0x184',
    'humanoid::health' => '0x188',
    'humanoid::hip_height' => '0x194',
    'humanoid::jump_height' => '0x1a0',
    'humanoid::jump_power' => '0x1a4',
    'humanoid::max_health' => '0x1a8',
    'humanoid::max_slope_angle' => '0x1ac',
    'humanoid::rig_type' => '0x1c0',
    'humanoid::walk_speed' => '0x1d0',
    'humanoid::auto_rotate' => '0x1d5',
    'humanoid::jump' => '0x1da',
    'humanoid::humanoid_state' => '0x898',
    'humanoid::walk_speed_check' => '0x3bc',

    'value_bool::value' => '0xb8',
    'value_int::value' => '0xb8',
    'value_number::value' => '0xb8',

    'instance::attribute_container' => '0x48',
    'instance::attribute_list' => '0x18',
    'instance::attribute_to_next' => '0x58',
    'instance::attribute_to_value' => '0x18',
    'instance::children_end' => '0x8',
    'instance::children_start' => '0x70',
    'instance::class_base' => '0x230',
    'instance::class_descriptor' => '0x18',
    'instance::class_name' => '0x8',
    'instance::name' => '0x98',
    'instance::parent' => '0x68',
    'instance::current_camera' => '0x488',
    'instance::children_stride' => '0x10',

    'gui_object::background_color3' => '0x540',
    'gui_object::border_color3' => '0x54c',
    'gui_object::image' => '0x988',
    'gui_object::layout_order' => '0x580',
    'gui_object::position' => '0x510',
    'gui_object::frame_position_x' => '0x510',
    'gui_object::frame_position_y' => '0x518',
    'gui_object::rich_text' => '0xb50',
    'gui_object::rotation' => '0x178',
    'gui_object::screen_gui_enabled' => '0x4c4',
    'gui_object::size' => '0x530',
    'gui_object::text' => '0xda0',
    'gui_object::text_color3' => '0xe50',
    'gui_object::text_color3_fallback' => '0xe50',
    'gui_object::visible' => '0x5ad',

    'mouse_service::input_object' => '0xf0',
    'mouse_service::mouse_position' => '0xd4',

    'camera::position' => '0xfc',
    'camera::rotation' => '0xd8',
    'camera::subject' => '0xc8',
    'camera::position_offset' => '0x0',
    'camera::viewport' => '0x28c',
    'camera::viewport_size' => '0x2c8',

    'chat::is_focused' => '0x154',

    'lighting::ambient' => '0xc8',
    'lighting::brightness' => '0x110',
    'lighting::colorshift_bottom' => '0xe0',
    'lighting::colorshift_top' => '0xd4',
    'lighting::exposure_compensation' => '0x11c',
    'lighting::fog_color' => '0xec',
    'lighting::fog_end' => '0x124',
    'lighting::fog_start' => '0x128',
    'lighting::geographic_latitude' => '0x180',
    'lighting::outdoor_ambient' => '0xf8',

    'sky::moon_angular_size' => '0x244',
    'sky::moon_texture_id' => '0xc8',
    'sky::skybox_bk' => '0xf8',
    'sky::skybox_dn' => '0x128',
    'sky::skybox_ft' => '0x158',
    'sky::skybox_lf' => '0x188',
    'sky::skybox_orientation' => '0x238',
    'sky::skybox_rt' => '0x1b8',
    'sky::skybox_up' => '0x1e8',
    'sky::star_count' => '0x248',
    'sky::sun_angular_size' => '0x23c',
    'sky::sun_texture_id' => '0x218',

    'mesh_part::mesh_id' => '0x290',
    'mesh_part::special_mesh_id' => '0xf8',

    'team::team_color' => '0xb8',

    'rbx_string::length' => '0x10',

    'workspace::gravity' => '0x210',
    'workspace::gravity_container' => '0x3e0',
    'workspace::primitives_pointer1' => '0x3e0',
    'workspace::primitives_pointer2' => '0x288',

    'replicator::nextgen_replicator' => '0x7B48E88',
    'fflags::target_time_delay_facctor_tenths' => '0x6B1CBA8',
];


$response = [
    'status' => 'success',
    'offsets' => $offsets,
    'sumtin' => $sumtin,
    'timestamp' => time()
];

echo customEncrypt(json_encode($response), $secret_key);
