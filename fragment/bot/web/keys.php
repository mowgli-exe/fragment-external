<?php

            function getKeyAndHWIDPairs($api_key) {
                if ($api_key !== 'urapikey') {
                    return json_encode(array('error' => 'Invalid API key'));
                }
                
                return '[]';
            }
    ?>