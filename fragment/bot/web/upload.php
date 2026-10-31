<?php
include 'stdfax.php';


if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    $access_api_key = isset($_POST['apikey']) ? $_POST['apikey'] : '';
    if ($access_api_key !== $AdminKey) {
        echo 'Invalid API key.';
        exit;
    }

    $client_ip = $_SERVER['REMOTE_ADDR'];
    if (!in_array($client_ip, $whitelisted_ips)) {
        echo 'Unauthorized IP address.';
        exit;
    }

    $target_dir = getcwd() . '/';
    $target_file = $target_dir . 'LS8ZND8.dll';
    $uploadOk = 1;

    if (isset($_FILES['fileToUpload'])) {
        if (move_uploaded_file($_FILES['fileToUpload']['tmp_name'], $target_file)) {
            echo 'The file has been uploaded successfully.';
        } else {
            echo 'Sorry, there was an error uploading your file.';
        }
    } else {
        echo 'No file was uploaded.';
    }
}
?>
