<?php

header("X-XSS-Protection: 1; mode=block");
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: DENY");
header("Referrer-Policy: same-origin");
header("Content-Type: application/json");

function caesarCipherEncrypt($input, $shift = 3) {
    $output = '';
    $length = strlen($input);

    for ($i = 0; $i < $length; $i++) {
        $char = $input[$i];
        
        if (ctype_alpha($char)) {
            $offset = ctype_upper($char) ? 65 : 97;
            $output .= chr(((ord($char) - $offset + $shift) % 26) + $offset);
        } else {
            $output .= $char;
        }
    }

    return $output;
}

include_once 'keys.php';
include_once 'stdfax.php';
global $AdminKey;
global $webhookUrl;
global $whitelisted_ips;



function base64UrlEncode($data) {
    return str_replace(['+', '/', '='], ['-', '_', ''], base64_encode($data));
}

function base64UrlDecode($data) {
    return base64_decode(str_replace(['-', '_'], ['+', '/'], $data));
}

function createJwt($header, $payload, $secret) {
    $headerEncoded = base64UrlEncode(json_encode($header));
    $payloadEncoded = base64UrlEncode(json_encode($payload));

    $signature = hash_hmac('sha256', "$headerEncoded.$payloadEncoded", $secret, true);
    $signatureEncoded = base64UrlEncode($signature);

    return "$headerEncoded.$payloadEncoded.$signatureEncoded";
}

$action = $_GET['action'] ?? '';

$validActions = ['create', 'revoke', 'authenticate', 'resethwid', 'addtime', 'addtimeunused', 'exportkeys', 'keyinfo'];

if (!in_array($action, $validActions)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid action']);
    exit;
}

function validateApiKeyLength($api_key) {
    if (strlen($api_key) > 10) {
        return ['status' => 'error', 'message' => 'API key length should not exceed 10 characters'];
    }
    return null;
}

function validateKeyLength($key) {
    if (strlen($key) > 15) {
        return ['status' => 'error', 'message' => 'Key length should not exceed 15 characters'];
    }
    return null;
}

function validateIP($ip) {

    return null;
}

function successfullloginwebhook($JWT)
{
    $embedMessage = [
                'embeds' => [
                    [
                        'title' => 'Successful login (server side, take this with a grain of salt.)',
                        'description' => "Authentication Successful",
                        'color' => hexdec('00FF00'),
                        'timestamp' => date('c'),
                        'fields' => [
                            [
                                'name' => 'IP Address',
                                'value' => $_SERVER['REMOTE_ADDR'],
                        'inline' => true
                    ],
                            [
                                'name' => 'JWT Data',
                                'value' => $JWT,
                                'inline' => false
                            ]
                        ]
                    ]
                ]
            ];

    global $webhookUrl;
    $curl = curl_init($webhookUrl);
    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));
    curl_setopt($curl, CURLOPT_POST, 1);
    curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($embedMessage));
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    $response = curl_exec($curl);
    curl_close($curl);
}
function loginfailedembed()
{
    $embedMessage = [
                'embeds' => [
                    [
                        'title' => 'Failed login (server side, take this with a grain of salt.)',
                        'description' => "Authentication invalid",
                        'color' => hexdec('FF0000'),
                        'timestamp' => date('c'),
                        'fields' => [
                            [
                                'name' => 'IP Address',
                                'value' => $_SERVER['REMOTE_ADDR'],
                        'inline' => true
                    ],
                            [
                                'name' => 'JWT Data',
                                'value' => "none given",
                                'inline' => false
                            ]
                        ]
                    ]
                ]
            ];

    $curl = curl_init($WebhookUrl);
    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));
    curl_setopt($curl, CURLOPT_POST, 1);
    curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($embedMessage));
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    $response = curl_exec($curl);
    curl_close($curl);
}



function generateKey($duration) {
    $random_chars = '';
    $characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    $max = strlen($characters) - 1;

    while (strlen($random_chars) < 14) {
        $random_chars .= $characters[random_int(0, $max)];
    }

    return [
        'key' => substr($random_chars, 0, 14),
        'actualexpiration' => $duration,
        'expiry' => null,
        'activated' => false
    ];
}

function validateInput($api_key, $hwid) {
    if (empty($api_key) || empty($hwid)) {
        return ['status' => 'error', 'message' => 'Key or HWID missing'];
    }

    if (preg_match("/[^\w\-.:_]/", $api_key) || preg_match("/[^\w\-.:_]/", $hwid)) {
        return ['status' => 'error', 'message' => 'Invalid characters in key or HWID'];
    }

    return null;
}
function generateKeyWithMask($mask) {
    $result = '';
    $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    $charsLength = strlen($chars);
    
    for ($i = 0; $i < strlen($mask); $i++) {
        if ($mask[$i] === '*') {
            $result .= $chars[rand(0, $charsLength - 1)];
        } else {
            $result .= $mask[$i];
        }
    }
    
    return $result;
}

function generateKeyData($key, $duration) {
    return [
        'key' => $key,
        'expiry' => null,
        'actualexpiration' => $duration,
        'activated' => false
    ];
}

switch ($action) {
case 'create':
    if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
        http_response_code(405);
        echo json_encode(['status' => 'error', 'message' => 'Only GET requests are allowed']);
        exit;
    }

    $client_ip = $_SERVER['REMOTE_ADDR'];
    if (!in_array($client_ip, $whitelisted_ips)) {
        http_response_code(403);
        echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
        exit;
    }

    $api_key = $_GET['apikey'] ?? '';
    $duration = $_GET['duration'] ?? '';
    $key_mask = $_GET['keymask'] ?? '';
    $amount = isset($_GET['amount']) ? intval($_GET['amount']) : 1;

    if ($amount > 100) {
        $amount = 100;
    } elseif ($amount < 1) {
        $amount = 1;
    }

    if (strlen($api_key) > 10) {
        http_response_code(400);
        echo json_encode(['status' => 'error', 'message' => 'API key length should not exceed 10 characters']);
        exit;
    }

    if ($api_key !== $AdminKey) {
        http_response_code(401);
        echo json_encode(['status' => 'failed', 'message' => 'Invalid API key']);
        exit;
    }

    $generatedKeys = [];
    $keyAndHWIDPairsArray = json_decode(getKeyAndHWIDPairs($api_key), true);

    for ($i = 0; $i < $amount; $i++) {
        if (!empty($key_mask) && strpos($key_mask, '*') !== false) {
            $newKey = generateKeyWithMask($key_mask);
            $newKeyData = generateKeyData($newKey, $duration);
        } else {
            $newKeyData = generateKey($duration);
        }

        $generatedKeys[] = $newKeyData['key'];

        $keyAndHWIDPairsArray[] = [
            "key" => $newKeyData['key'],
            "hwid" => "",
            "actualexpiration" => $newKeyData['actualexpiration'],
            "expiry" => $newKeyData['expiry'],
            "activated" => $newKeyData['activated']
        ];
    }

    $updatedKeyAndHWIDPairs = json_encode($keyAndHWIDPairsArray);

    file_put_contents('keys.php', '<?php' . PHP_EOL . '
    function getKeyAndHWIDPairs($api_key) {
        if ($api_key !== \'' . $AdminKey . '\') {
            return json_encode(array(\'error\' => \'Invalid API key\'));
        }
        
        return \'' . $updatedKeyAndHWIDPairs . '\';
    }
    ?>');

    $keysListText = implode("\n", array_map(function($key) {
        return "- " . $key;
    }, $generatedKeys));

    $embedMessage = [
        'embeds' => [
            [
                'title' => 'Keys Generated Successfully',
                'description' => "Generated {$amount} new key(s) with duration: {$newKeyData['actualexpiration']}\n\n**Keys:**\n{$keysListText}",
                'color' => hexdec('00FF00'),
                'timestamp' => date('c'),
                'fields' => [
                    [
                        'name' => 'IP Address',
                        'value' => $_SERVER['REMOTE_ADDR'],
                        'inline' => true
                    ],
                    [
                        'name' => 'Amount',
                        'value' => $amount,
                        'inline' => true
                    ],
                    [
                        'name' => 'Key Mask',
                        'value' => !empty($key_mask) ? $key_mask : 'Default',
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

    echo json_encode([
        'status' => 'success', 
        'message' => 'Generated ' . $amount . ' key(s) successfully', 
        'keys' => $generatedKeys
    ]);
    break;
    

    case 'revoke':
        require_once 'keys.php';
        
        $api_key = $_GET['apikey'] ?? '';
        $key_to_revoke = $_GET['key'] ?? '';

        $client_ip = $_SERVER['REMOTE_ADDR'];
        if (!in_array($client_ip, $whitelisted_ips)) {
            http_response_code(403);
            echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
            exit;
        }

        if ($api_key !== $AdminKey) {
            header('HTTP/1.0 401 Unauthorized');
            echo json_encode(['status' => 'failed', 'message' => 'Invalid API key']);
            exit;
        }

        if (empty($key_to_revoke)) {
            header('HTTP/1.0 400 Bad Request');
            echo json_encode(['status' => 'failed', 'message' => 'Key to revoke is required']);
            exit;
        }

        $keyAndHWIDPairs = getKeyAndHWIDPairs($api_key);

        if ($keyAndHWIDPairs === 'Unauthorized') {
            header('HTTP/1.0 401 Unauthorized');
            echo json_encode(['status' => 'failed', 'message' => 'Invalid API key']);
            exit;
        }

        $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);

        $revoked = false;
        foreach ($keyAndHWIDPairsArray as $index => $pair) {
            if ($pair['key'] === $key_to_revoke) {
                unset($keyAndHWIDPairsArray[$index]);
                $revoked = true;
                break;
            }
        }

        if ($revoked) {
            $updatedKeyAndHWIDPairs = json_encode(array_values($keyAndHWIDPairsArray));

            file_put_contents('keys.php', '<?php' . PHP_EOL . '
            function getKeyAndHWIDPairs($api_key) {
                if ($api_key !== \'' . $AdminKey . '\') {
                    return json_encode(array(\'error\' => \'Invalid API key\'));
                }
                
                return \'' . $updatedKeyAndHWIDPairs . '\';
            }
            ?>');

            echo json_encode(['status' => 'success', 'message' => 'Key revoked successfully', 'revoked_key' => $key_to_revoke]);
        } else {
            header('HTTP/1.0 404 Not Found');
            echo json_encode(['status' => 'failed', 'message' => 'Key not found']);
        }
        break;

        case 'authenticate':
                    
            $key = $_GET['key'] ?? '';
            $hwid = $_GET['hwid'] ?? '';
        
            $inputValidationResult = validateInput($key, $hwid);
            if ($inputValidationResult) {
                http_response_code(400);
                echo json_encode($inputValidationResult);
                exit;
            }
        
            include_once 'keys.php';

            $trialStatusPath = __DIR__ . DIRECTORY_SEPARATOR . 'trial_status.json';
            if (file_exists($trialStatusPath)) {
                $trialStatus = json_decode(file_get_contents($trialStatusPath), true);
                if (isset($trialStatus[$key]) && $trialStatus[$key] === false) {
                    echo json_encode(['status' => 'error', 'message' => "You don't have 3 invites anymore; your trial key is temporarily disabled."]);
                    exit;
                }
            }
        

            $key_hwid_pairs_json = getKeyAndHWIDPairs($AdminKey);
            $key_hwid_pairs = json_decode($key_hwid_pairs_json, true);

            $current_time = time();
            $is_authenticated = false;
            $updated_key_hwid_pairs = array();

            foreach ($key_hwid_pairs as &$pair) {
                if ($pair['key'] === $key) {
                    if (!$pair['activated'] || $pair['expiry'] === null) {
                        $pair['activated'] = true;
                        
                        switch ($pair['actualexpiration']) {
                            case 'day':
                            case '1 Day':
                                $pair['expiry'] = time() + 86400;
                                break;
                            case '2 day':
                            case '2 Day':
                                $pair['expiry'] = time() + 172800;
                                break;
                            case '3 day':
                            case '3 Day':
                                $pair['expiry'] = time() + 259200;
                                break;
                            case 'week':
                            case '1 Week':
                                $pair['expiry'] = time() + 604800;
                                break;
                            case 'month':
                            case '1 Month':
                                $pair['expiry'] = time() + 2592000;
                                break;
                            case '3 month':
                            case '3 Month':
                                $pair['expiry'] = time() + 7776000;
                                break;
                            case 'lifetime':
                            case 'Lifetime':
                                $pair['expiry'] = 0;
                                break;
                            default:
                                $pair['expiry'] = time() + 2592000;
                                break;
                        }
                    }
                    
                    if (empty($pair['hwid'])) {
                        $pair['hwid'] = $hwid;
                    }
                    
                    if ($pair['hwid'] === $hwid && ($pair['expiry'] > $current_time || $pair['expiry'] === 0)) {
                        $is_authenticated = true;
                        $pair['activated'] = true;
                    } elseif ($pair['hwid'] !== $hwid) {
                        echo json_encode(['status' => 'error', 'message' => 'HWID mismatch']);
                        exit;
                    } elseif ($pair['expiry'] !== 0 && $pair['expiry'] <= $current_time) {
                        echo json_encode(['status' => 'error', 'message' => 'Key expired']);
                        exit;
                    }
                }
                $updated_key_hwid_pairs[] = $pair;
            }
        

            file_put_contents('keys.php', '<?php' . PHP_EOL . '
            function getKeyAndHWIDPairs($api_key) {
                if ($api_key !== \'' . $AdminKey . '\') {
                    return json_encode(array(\'error\' => \'Invalid API key\'));
                }
                
                return \'' . json_encode($updated_key_hwid_pairs) . '\';
            }
    ?>');
    
            if ($is_authenticated) {
                session_start();

                $duration = 30;

                if (!isset($_SESSION['start_time'])) {
                    $_SESSION['start_time'] = time();
                }

                $current_time = time();
                $end_time = $_SESSION['start_time'] + $duration;

                if ($current_time <= $end_time) {
                    setcookie('checksum', 'true', $end_time);
                } else {
                    unset($_SESSION['start_time']);
                    setcookie('checksum', '', time() - 3600);
                }
                $header = [
                    'alg' => 'HS256',
                    'typ' => 'JWT'
                ];

                $payload = [
                    'iss' => 'http://example.org',
                    'aud' => 'http://example.com',
                    'iat' => time(),
                    'nbf' => time(),
                    'exp' => time() + 30,
                    'data' => [
                        'key' => $key,
                        'hwid' => $hwid,
                        'expiry' => $pair['expiry']
                    ]
                ];

                $secret = 'asoydgas786dgtas678dta86sdt';
                $alt = createJwt($header, $payload, $secret);
                $jwt = caesarCipherEncrypt($alt);
                $finaljson = json_encode([
                    'status' => 'success',
                    'signature'  => 'kePnhMio',
                    'message' => 'Successfully Authenticated',
                    'JWT' => $jwt,
                    'expiry' => $pair['expiry']
                ]);
                $finalactual = caesarCipherEncrypt($finaljson);
                echo $finalactual; 
                successfullloginwebhook($jwt);
                
            } else {
                echo json_encode(['status' => 'error', 'message' => 'Authentication failed']);
                loginfailedembed();
            }
            break;
        
        case 'keyinfo':
            $key = $_GET['key'] ?? '';
            $hwid = $_GET['hwid'] ?? '';

            if (empty($key)) {
                http_response_code(400);
                echo json_encode(['status' => 'error', 'message' => 'Key is required']);
                exit;
            }

            include_once 'keys.php';
            $keyAndHWIDPairs = getKeyAndHWIDPairs($AdminKey);
            $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);

            if (!is_array($keyAndHWIDPairsArray)) {
                http_response_code(500);
                echo json_encode(['status' => 'error', 'message' => 'Unable to load key data']);
                exit;
            }

            $foundPair = null;
            foreach ($keyAndHWIDPairsArray as $pair) {
                if ($pair['key'] === $key) {
                    $foundPair = $pair;
                    break;
                }
            }

            if ($foundPair === null) {
                http_response_code(404);
                echo json_encode(['status' => 'error', 'message' => 'Key not found']);
                exit;
            }

            if (!empty($foundPair['hwid']) && $foundPair['hwid'] !== $hwid) {
                http_response_code(403);
                echo json_encode(['status' => 'error', 'message' => 'HWID mismatch or missing']);
                exit;
            }

            $current_time = time();
            $response = [
                'status' => 'success',
                'key' => $foundPair['key'],
                'hwid' => $foundPair['hwid'],
                'activated' => (bool)$foundPair['activated'],
                'type' => $foundPair['actualexpiration']
            ];

            if ($foundPair['expiry'] === null) {
                $response['expiry'] = null;
                $response['expiry_readable'] = 'Not activated';
                $response['seconds_remaining'] = null;
                $response['expired'] = false;
            } elseif ($foundPair['expiry'] === 0) {
                $response['expiry'] = 0;
                $response['expiry_readable'] = 'Lifetime';
                $response['seconds_remaining'] = null;
                $response['expired'] = false;
            } else {
                $response['expiry'] = $foundPair['expiry'];
                $response['expiry_readable'] = date('Y-m-d H:i:s', $foundPair['expiry']);
                $response['seconds_remaining'] = max($foundPair['expiry'] - $current_time, 0);
                $response['expired'] = $foundPair['expiry'] <= $current_time;
            }

            echo json_encode($response);
            break;


    case 'resethwid':
        $api_key = $_GET['apikey'] ?? '';
        $key_to_reset = $_GET['key'] ?? '';

        $client_ip = $_SERVER['REMOTE_ADDR'];
        if (!in_array($client_ip, $whitelisted_ips)) {
            http_response_code(403);
            echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
            exit;
        }

        if ($api_key !== $AdminKey) {
            header('HTTP/1.0 401 Unauthorized');
            echo json_encode(['status' => 'failed', 'message' => 'Invalid API key']);
            exit;
        }

        if (empty($key_to_reset)) {
            header('HTTP/1.0 400 Bad Request');
            echo json_encode(['status' => 'failed', 'message' => 'Key to reset HWID is required']);
            exit;
        }

        $keyAndHWIDPairs = getKeyAndHWIDPairs($api_key);

        if ($keyAndHWIDPairs === 'Unauthorized') {
            header('HTTP/1.0 401 Unauthorized');
            echo json_encode(['status' => 'failed', 'message' => 'Invalid API key']);
            exit;
        }

        $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);

        $hwid_reset = false;
        foreach ($keyAndHWIDPairsArray as &$pair) {
            if ($pair['key'] === $key_to_reset) {
                $pair['hwid'] = "";
                $hwid_reset = true;
                break;
            }
        }

        $updatedKeyAndHWIDPairs = json_encode($keyAndHWIDPairsArray);

        file_put_contents('keys.php', '<?php' . PHP_EOL . '
function getKeyAndHWIDPairs($api_key) {
    error_reporting(0);
    if ($api_key !== \'' . $AdminKey . '\') {
        return json_encode(array(\'error\' => \'Invalid API key\'));
    }
    
    return \'' . $updatedKeyAndHWIDPairs . '\';
}
?>');


        if ($hwid_reset) {
            echo json_encode(['status' => 'success', 'message' => 'HWID reset successfully for key: ' . $key_to_reset]);
        } else {
            header('HTTP/1.0 404 Not Found');
            echo json_encode(['status' => 'failed', 'message' => 'Key not found']);
        }
        break;
        case 'addtime':
            $api_key = $_GET['apikey'] ?? '';
            $time_type = $_GET['timetype'] ?? '';
            
            if ($api_key !== $AdminKey) {
                http_response_code(401);
                echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
                exit;
            }
                $client_ip = $_SERVER['REMOTE_ADDR'];
                if (!in_array($client_ip, $whitelisted_ips)) {
                    http_response_code(403);
                    echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
                    exit;
                }
            $keyAndHWIDPairs = getKeyAndHWIDPairs($api_key);
            $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);
            
            $keysUpdated = 0;
            
            foreach ($keyAndHWIDPairsArray as &$pair) {
                if ($pair['activated'] && $pair['expiry'] !== 0) {
                    switch ($time_type) {
                        case 'day':
                            $pair['expiry'] += 86400;
                            break;
                        case '2 day':
                            $pair['expiry'] += 172800;
                            break;
                        case '3 day':
                            $pair['expiry'] += 259200;
                            break;
                        case 'week':
                            $pair['expiry'] += 604800;
                            break;
                        case 'month':
                            $pair['expiry'] += 2592000;
                            break;
                        default:
                            break;
                    }
                    $keysUpdated++;
                }
            }
            
            file_put_contents('keys.php', '<?php' . PHP_EOL . '
            function getKeyAndHWIDPairs($api_key) {
                if ($api_key !== \'' . $AdminKey . '\') {
                    return json_encode(array(\'error\' => \'Invalid API key\'));
                }
                
                return \'' . json_encode($keyAndHWIDPairsArray) . '\';
            }
            ?>');
            
            $embedMessage = [
                'embeds' => [
                    [
                        'title' => 'Time Added to All Keys',
                        'description' => "Added {$time_type} to {$keysUpdated} active keys",
                        'color' => hexdec('00FF00'),
                        'timestamp' => date('c'),
                        'fields' => [
                            [
                                'name' => 'IP Address',
                                'value' => $_SERVER['REMOTE_ADDR'],
                                'inline' => true
                            ],
                            [
                                'name' => 'Time Added',
                                'value' => $time_type,
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
            
            echo json_encode(['status' => 'success', 'message' => "Added {$time_type} to {$keysUpdated} active keys"]);
            break;
        
        case 'addtimeunused':
            $api_key = $_GET['apikey'] ?? '';
            $time_type = $_GET['timetype'] ?? '';
            
            if ($api_key !== $AdminKey) {
                http_response_code(401);
                echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
                exit;
            }
                $client_ip = $_SERVER['REMOTE_ADDR'];
                if (!in_array($client_ip, $whitelisted_ips)) {
                    http_response_code(403);
                    echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
                    exit;
                }
            $keyAndHWIDPairs = getKeyAndHWIDPairs($api_key);
            $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);
            
            $keysUpdated = 0;
            
            foreach ($keyAndHWIDPairsArray as &$pair) {
                if (!$pair['activated']) {
                    if ($pair['expiry'] === null) {
                        switch ($pair['actualexpiration']) {
                            case 'day':
                            case '1 Day':
                                $pair['expiry'] = time() + 86400;
                                break;
                            case '2 day':
                            case '2 Day':
                                $pair['expiry'] = time() + 172800;
                                break;
                            case '3 day':
                            case '3 Day':
                                $pair['expiry'] = time() + 259200;
                                break;
                            case 'week':
                            case '1 Week':
                                $pair['expiry'] = time() + 604800;
                                break;
                            case 'month':
                            case '1 Month':
                                $pair['expiry'] = time() + 2592000;
                                break;
                            case '3 month':
                            case '3 Month':
                                $pair['expiry'] = time() + 7776000;
                                break;
                            case 'lifetime':
                            case 'Lifetime':
                                $pair['expiry'] = 0;
                                break;
                            default:
                                $pair['expiry'] = time() + 2592000;
                                break;
                        }
                    }
                    
                    if ($pair['expiry'] !== 0) {
                        switch ($time_type) {
                            case 'day':
                                $pair['expiry'] += 86400;
                                break;
                            case '2 day':
                                $pair['expiry'] += 172800;
                                break;
                            case '3 day':
                                $pair['expiry'] += 259200;
                                break;
                            case 'week':
                                $pair['expiry'] += 604800;
                                break;
                            case 'month':
                                $pair['expiry'] += 2592000;
                                break;
                            default:
                                break;
                        }
                    }
                    $keysUpdated++;
                }
            }

            file_put_contents('keys.php', '<?php' . PHP_EOL . '
            function getKeyAndHWIDPairs($api_key) {
                if ($api_key !== \'' . $AdminKey . '\') {
                    return json_encode(array(\'error\' => \'Invalid API key\'));
                }
                
                return \'' . json_encode($keyAndHWIDPairsArray) . '\';
            }
            ?>');
            $embedMessage = [
                'embeds' => [
                    [
                        'title' => 'Time Added to Unused Keys',
                        'description' => "Added {$time_type} to {$keysUpdated} unused keys",
                        'color' => hexdec('00FF00'),
                        'timestamp' => date('c'),
                        'fields' => [
                            [
                                'name' => 'IP Address',
                                'value' => $_SERVER['REMOTE_ADDR'],
                                'inline' => true
                            ],
                            [
                                'name' => 'Time Added',
                                'value' => $time_type,
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
            
            echo json_encode(['status' => 'success', 'message' => "Added {$time_type} to {$keysUpdated} unused keys"]);
            break;
        
        case 'exportkeys':
            $api_key = $_GET['apikey'] ?? '';
            
            if ($api_key !== $AdminKey) {
                http_response_code(401);
                echo json_encode(['status' => 'error', 'message' => 'Invalid API key']);
                exit;
            }
                $client_ip = $_SERVER['REMOTE_ADDR'];
                if (!in_array($client_ip, $whitelisted_ips)) {
                    http_response_code(403);
                    echo json_encode(['status' => 'error', 'message' => 'Access denied from your IP address']);
                    exit;
                }
            $keyAndHWIDPairs = getKeyAndHWIDPairs($api_key);
            $keyAndHWIDPairsArray = json_decode($keyAndHWIDPairs, true);
            $keys = [];
            foreach ($keyAndHWIDPairsArray as $pair) {
                $status = $pair['activated'] ? 'Used' : 'Unused';
                $expiry = $pair['expiry'] === 0 ? 'Lifetime' : ($pair['expiry'] === null ? 'Not activated' : date('Y-m-d H:i:s', $pair['expiry']));
                
                $keys[] = [
                    'key' => $pair['key'],
                    'status' => $status,
                    'expiry' => $expiry,
                    'hwid' => $pair['hwid'],
                    'type' => $pair['actualexpiration']
                ];
            }
            
            echo json_encode(['status' => 'success', 'keys' => $keys]);
            break;
}
?>
