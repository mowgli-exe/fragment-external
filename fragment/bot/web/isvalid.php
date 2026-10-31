<?php

include 'keys.php';
include 'stdfax.php';

function getKeyInfo($api_key, $key) {
    $keyPairs = getKeyAndHWIDPairs($api_key);
    
    $keyInfo = null;
    foreach(json_decode($keyPairs) as $pair) {
        if ($pair->key === $key) {
            $keyInfo = $pair;
            break;
        }
    }
    
    if ($keyInfo === null) {
        return json_encode(array('error' => 'Key not found'));
    }
    
    return json_encode(array('success' => 'valid'));
}

if (isset($_GET['api_key']) && isset($_GET['key'])) {
    $api_key = $_GET['api_key'];
    $key = $_GET['key'];
    
    if ($api_key !== "T2hQ9z") {
        echo json_encode(array('error' => 'Invalid API key'));
        exit;
    }
    
    $info = getKeyInfo($AdminKey, $key);
    
    header("X-XSS-Protection: 1; mode=block");
    header("X-Content-Type-Options: nosniff");
    header("X-Frame-Options: DENY");
    header("Referrer-Policy: same-origin");
    header("Content-Type: application/json");
    
    error_reporting(0);
    
    echo $info;
} else {
    echo json_encode(array('error' => 'Missing parameters'));
}
?>
