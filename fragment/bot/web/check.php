<?php
require_once 'keys.php';

function encryptData($data) {
    $key = '8e7d6c5b4a39281f0e9d8c7b6a59483f2e1d0c9b8a79685f4e3d2c1b0a9';
    $salt = bin2hex(random_bytes(16));

    $encrypted = '';
    $keyLen = strlen($key);
    $saltLen = strlen($salt);
    $dataLen = strlen($data);

    for ($round = 0; $round < 3; $round++) {
        $roundKey = '';

        for ($i = 0; $i < $dataLen; $i++) {
            $keyPos = ($i + $round * 13) % $keyLen;
            $saltPos = ($i + $round * 7) % $saltLen;
            $roundKey .= chr((ord($key[$keyPos]) ^ ord($salt[$saltPos]) ^ ($round * 31)) & 0xFF);
        }

        $temp = '';
        for ($i = 0; $i < $dataLen; $i++) {
            $temp .= chr(ord($data[$i]) ^ ord($roundKey[$i]) ^ ($i * 17) & 0xFF);
        }
        $data = $temp;

        $scrambled = '';
        for ($i = 0; $i < $dataLen; $i++) {
            $scramblePos = ($i * 97 + $round * 23) % $dataLen;
            $scrambled .= $data[$scramblePos];
        }
        $data = $scrambled;
    }

    $padding = bin2hex(random_bytes(8));

    return $salt . bin2hex($data) . $padding;
}

if (!function_exists('getKeyAndHWIDPairs')) {
    die('Error: keys.php not properly loaded or PHP not processing correctly');
}

if (!isset($_GET['key']) || empty($_GET['key'])) {
    http_response_code(400);
    die('Error: No key provided. Usage: ?key=YOUR_KEY');
}

$provided_key = $_GET['key'];
$api_key = 'urapikey';

try {
    $key_pairs_json = getKeyAndHWIDPairs($api_key);

    if (!$key_pairs_json || is_array(json_decode($key_pairs_json, true)) === false) {
        http_response_code(500);
        die('Error: Failed to retrieve key pairs');
    }

    $key_pairs = json_decode($key_pairs_json, true);

    if (json_last_error() !== JSON_ERROR_NONE) {
        http_response_code(500);
        die('Error: Invalid JSON data from key pairs');
    }

    $key_found = false;
    foreach ($key_pairs as $pair) {
        if (isset($pair['key']) && $pair['key'] === $provided_key) {
            $key_found = true;
            break;
        }
    }

    if (!$key_found) {
        http_response_code(401);
        die('Error: Invalid key');
    }

    $file_path = __DIR__ . '/AOIUSDHJA980SYHDA9WS8UDH9A8SHD9A8SJD.BIN';

    if (!file_exists($file_path)) {
        http_response_code(500);
        die('Error: File not found on server');
    }

    $file_contents = file_get_contents($file_path);
    if ($file_contents === false) {
        http_response_code(500);
        die('Error: Failed to read file contents');
    }

    if (ob_get_length()) {
        ob_clean();
    }

    $Output = bin2hex($file_contents);
    $Encryption = encryptData($Output);
    header('Content-Type: text/plain; charset=utf-8');
    echo $Encryption;
    exit;

} catch (Exception $e) {
    http_response_code(500);
    die('Error: ' . $e->getMessage());
}
