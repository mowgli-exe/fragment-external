<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

$Compare = 'T2hQ9z';
$AdminKey = 'urapikey';
$webhookUrl = 'urwebhook';

function validateKeyLength($key) {


    return null;
}

include_once 'keys.php';

$api_key = $_GET['apikey'] ?? '';
$key_to_reset = $_GET['key'] ?? '';

if ($api_key !== $Compare) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
    exit;
}

if (empty($key_to_reset)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Key to reset HWID is required']);
    exit;
}

$keyLengthValidationResult = validateKeyLength($key_to_reset);
if ($keyLengthValidationResult) {
    http_response_code(400);
    echo json_encode($keyLengthValidationResult);
    exit;
}

$keyAndHWIDPairs = getKeyAndHWIDPairs($AdminKey);

if (isset(json_decode($keyAndHWIDPairs, true)['error'])) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
    exit;
}

$keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);

$hwid_reset = false;
foreach ($keyAndHWIDPairsArray as &$pair) {
    if ($pair['key'] === $key_to_reset) {
        $old_hwid = $pair['hwid'];
        $pair['hwid'] = "";
        $hwid_reset = true;
        break;
    }
}

if ($hwid_reset) {
    $updatedKeyAndHWIDPairs = json_encode($keyAndHWIDPairsArray);
    
    file_put_contents('keys.php', '<?php' . PHP_EOL . '
function getKeyAndHWIDPairs($AdminKey) {
    if ($AdminKey !== \'' . $AdminKey . '\') {
        return json_encode(array(\'error\' => \'Invalid API key\'));
    }
    
    return \'' . $updatedKeyAndHWIDPairs . '\';
}
?>' );

    if (!empty($webhookUrl)) {
        $embedMessage = [
            'embeds' => [
                [
                    'title' => 'HWID Reset',
                    'description' => "HWID has been reset for key: $key_to_reset",
                    'color' => hexdec('FFA500'),
                    'timestamp' => date('c'),
                    'fields' => [
                        [
                            'name' => 'IP Address',
                            'value' => $_SERVER['REMOTE_ADDR'],
                            'inline' => true
                        ],
                        [
                            'name' => 'Old HWID',
                            'value' => !empty($old_hwid) ? $old_hwid : 'Not Set',
                            'inline' => true
                        ]
                    ]
                ]
            ]
        ];
        
        $curl = curl_init($webhookUrl);
        curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));
        curl_setopt($curl, CURLOPT_POST, 1);
        curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($embedMessage));
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
        $response = curl_exec($curl);
        curl_close($curl);
    }

    echo json_encode(['status' => 'success', 'message' => 'HWID reset successfully for key: ' . $key_to_reset]);
} else {
    http_response_code(404);
    echo json_encode(['status' => 'error', 'message' => 'Key not found']);
}
?>
