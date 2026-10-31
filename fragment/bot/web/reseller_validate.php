<?php

include 'keys.php';
include 'stdfax.php';

$ResellerApiKey = "mR5kQ2";

header("X-XSS-Protection: 1; mode=block");
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: DENY");
header("Referrer-Policy: same-origin");
header("Content-Type: application/json");

function json_response($status, $message, $code = 200, $extra = array()) {
    http_response_code($code);
    echo json_encode(array_merge(array(
        'status' => $status,
        'message' => $message
    ), $extra));
    exit;
}

if (!isset($_GET['api_key']) || !isset($_GET['key'])) {
    json_response('error', 'Missing parameters', 400, array('valid' => false));
}

$api_key = trim((string)$_GET['api_key']);
$key = trim((string)$_GET['key']);

if ($api_key !== $ResellerApiKey && $api_key !== $AdminKey) {
    json_response('error', 'Invalid API key', 401, array('valid' => false));
}

if ($key === '' || strlen($key) > 64 || !preg_match('/^[A-Za-z0-9-]+$/', $key)) {
    json_response('error', 'Invalid key format', 400, array('valid' => false));
}

$key_pairs_json = getKeyAndHWIDPairs($AdminKey);
$key_pairs = json_decode($key_pairs_json, true);

if (!is_array($key_pairs)) {
    json_response('error', 'Failed to load key store', 500, array('valid' => false));
}

foreach ($key_pairs as $pair) {
    if (!is_array($pair) || !isset($pair['key'])) {
        continue;
    }

    if ((string)$pair['key'] !== $key) {
        continue;
    }

    $expiry = $pair['expiry'] ?? null;
    if ($expiry !== null && $expiry !== 0 && is_numeric($expiry) && (int)$expiry <= time()) {
        json_response('error', 'Key expired', 200, array('valid' => false));
    }

    json_response('success', 'Key is valid', 200, array(
        'valid' => true,
        'key' => (string)$pair['key'],
        'type' => (string)($pair['actualexpiration'] ?? 'unknown'),
        'activated' => (bool)($pair['activated'] ?? false),
        'expiry' => $expiry
    ));
}

json_response('error', 'Key not found', 200, array('valid' => false));

?>
