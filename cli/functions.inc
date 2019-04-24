<?php

use hng2_media\media_record;
use hng2_tools\cli_colortags;

/**
 * @param string  $url
 * @param string  $referer
 *
 * @return media_record|null
 * @throws Exception
 */
function get_featured_image($url, $referer = "")
{
    global $settings, $config, $media_repository, $post, $author;
    
    $res = $media_repository->find(array("description" => $url), 1, 0, "creation_date desc");
    if( count($res) )
    {
        $item = current($res);
        
        return  $item;
    }
    
    $filename  = basename(parse_url($url, PHP_URL_PATH));
    $extension = strtolower(end(explode(".", $filename)));
    
    cli_colortags::write("<light_blue> │ > Fetching main image from source...</light_blue>\n");
    
    if( ! in_array($extension, array("png", "jpg", "jpeg", "gif")) )
    {
        cli_colortags::write("<yellow> │ > $filename is not a valid image! Main image wont be set.</yellow>\n");
        
        return null;
    }
    
    $filename = wp_sanitize_filename(make_excerpt_of($post->title, 64, false)) . "." . $extension;
    $path     = "{$config->datafiles_location}/tmp/$filename";
    $file     = array(
        "name"     => $filename,
        "type"     => "image/$extension",
        "tmp_name" => $path,
        "error"    => null,
        "size"     => 0,
    );
    
    $user_agents = $settings->get("modules:vsimporter.user_agents");
    if( ! empty($user_agents) ) $user_agents = explode("\n", $user_agents);
    
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL,            $url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
    
    if( ! empty($user_agents) )
    {
        $user_agent = trim(array_rand($user_agents));
        curl_setopt($ch, CURLOPT_USERAGENT, $user_agent);
    }
    
    curl_setopt($ch, CURLOPT_REFERER, $referer);
    
    $contents = curl_exec($ch);
    sleep(1);
    
    # if( $arg["--debug"] ) echo "done. Response:\n$contents\n";
    
    if( curl_error($ch) )
    {
        $error = curl_error($ch);
        cli_colortags::write("<yellow> │ > Can't fetch image from {$url}: $error. Image ignored.</yellow>\n");
        
        return null;
    }
    
    curl_close($ch);
    
    if( empty($contents) )
    {
        cli_colortags::write("<yellow> │ > Image from {$url} is empty. Image ignored.</yellow>\n");
        
        return null;
    }
    
    if( ! @file_put_contents($file["tmp_name"], $contents) )
    {
        cli_colortags::write("<yellow> │ > Cannot save image {$file["tmp_name"]}. Image ignored.</yellow>\n");
        
        return null;
    }
    
    $file["size"] = filesize($file["tmp_name"]);
    
    $item_data = array(
        "title"          => make_excerpt_of($post->title, 64, false),
        "description"    => $url,
        "main_category"  => $post->main_category,
        "visibility"     => "public",
        "status"         => "published",
        "password"       => "",
        "allow_comments" => "1",
    );
    
    $res = $media_repository->receive_and_save($item_data, $file, true, true, $author);
    if( is_string($res) )
    {
        cli_colortags::write("<yellow> │ > Cannot save media item: $res. Image ignored.</yellow>\n");
        
        return null;
    }
    
    cli_colortags::write("<light_blue> │ > Done. Image id set to $res->id_media.</light_blue>\n");
    
    return $res;
}
