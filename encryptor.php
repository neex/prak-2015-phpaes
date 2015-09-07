<?php
    function error($err) {
        $err = urlencode($err);
        header("Location: index.html#${err}");
        exit;
    }

    if (!isset($_REQUEST["data"]) || !isset($_REQUEST["filename"]) || !is_string($_REQUEST["data"]) || !is_string($_REQUEST["filename"])) {
       error("bad request");
    }

    $data = $_REQUEST["data"];
    $filename = $_REQUEST["filename"];

    if (strlen($data) % 16 != 0) {
       error("data length must be multiple of 16");
    }

    if (!$filename) {
       $filename = "encrypted_".time().".bin";
    }

    $sock = fsockopen("localhost", 3456);
    if (!$sock) {
       error("error while connecting to backend");
    }

    fwrite($sock, $filename."\n".strlen($data)."\n".$data);

    $status = trim(fgets($sock));
    $details = trim(fgets($sock));
    fclose($sock);

    if ($status == "ERROR") {
       error($details);
    } else if ($status == "OK") {
       header("Content-Disposition: attachment; filename=\"${details}\"");
       header("Content-Type: application/octet-stream");
       $full_filename = "temporary_files/".$details;
       readfile($full_filename);
       unlink($full_filename);
    } else {
       error("cannot parse backend answer");
    }
?>