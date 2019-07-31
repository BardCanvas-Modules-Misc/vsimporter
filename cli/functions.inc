<?php

use hng2_base\account;
use hng2_media\media_record;
use hng2_tools\cli_colortags;

function set_category_and_tags()
{
    global $tags, $raw_content, $setting_categories, $post, $default_category_id, $category_ids_by_title;
    
    $hits    = array();
    $matches = array();
    
    foreach($setting_categories as $title => $keywords)
    {
        foreach($keywords as $keyword)
        {
            $keyword = strtolower($keyword);
            $kcount  = substr_count($raw_content, $keyword);
            
            if( $kcount == 0 ) continue;
            
            $matches[$title][$keyword] = $kcount;
            $hits[$title]++;
        }
    }
    
    if( empty($matches) )
    {
        cli_colortags::write(
            "<yellow> │   Warning: article didn't hit any category. Setting it to fallback.</yellow>\n"
        );
        $post->main_category = $default_category_id;
        
        find_featured_keywords();
        
        return;
    }
    
    foreach($hits as $title => $value)
    {
        $value = $value . "." . array_sum($matches[$title]);
        $hits[$title] = $value;
    }
    
    reset($hits); arsort($hits);
    $winner_category = key($hits);
    $score           = current($hits);
    $category_id     = $category_ids_by_title[$winner_category];
    cli_colortags::write(
        "<light_gray> │   Post category will be set to </light_gray><light_blue>$winner_category</light_blue> " .
        "<light_gray>(#$category_id). Score: </light_gray><light_blue>$score</light_blue>.</light_gray>\n"
    );
    $post->main_category = $category_id;
    
    # Adding hashtags
    foreach($matches[$winner_category] as $keyword => $kcount)
        $tags[] = str_replace(array(" ", "-"), "", ucwords($keyword));
    
    find_featured_keywords();
}

function find_featured_keywords()
{
    global $tags, $raw_content, $featured_keywords, $featured_tag;
    
    if( empty($featured_tag) ) return;
    
    $matches = array();
    foreach($featured_keywords as $keyword)
    {
        $keyword = strtolower($keyword);
        $kcount  = substr_count($raw_content, $keyword);
        
        if( $kcount == 0 ) continue;
        
        $matches[$keyword] = $kcount;
    }
    
    if( empty($matches) ) return;
    if( count($matches) < 3 ) return;
    
    cli_colortags::write(
        "<light_cyan> │   Post flagged as featured (keyword hits: #" . implode(", #", array_keys($matches)) . ")</light_cyan>\n"
    );
    
    $tags[] = $featured_tag;
}

function forge_author()
{
    global $fallback_account, $page, $user_level, $settings, $post, $mem_cache;
    
    $post_domain = current(explode("/", preg_replace('#http://|https://|www\.#i', "", $page->url)));
    if( empty($post_domain) )
    {
        cli_colortags::write("<yellow> │   Warning: couldn't extract domain for author check! Setting author to fallback account id.</yellow>\n");
        $author = new account($fallback_account);
    }
    else
    {
        $author_slug = wp_sanitize_filename($post_domain);
        $author      = new account($author_slug);
        if( $author->_exists )
        {
            if( $author->level != $user_level )
            {
                cli_colortags::write("<yellow> │   Warning: author account found, but it doesn't have the proper level! Fallback account will be used.</yellow>\n");
                $author = new account($fallback_account);
            }
        }
        else
        {
            $author->set_new_id();
            $author->user_name     = $author_slug;
            $author->display_name  = $post_domain;
            $author->email         = $settings->get("engine.webmaster_address");
            $author->password      = md5(randomPassword());
            $author->birthdate     = date("Y-m-d");
            $author->country       = "US";
            $author->homepage_url  = (stristr($page->url, "https://") === false ? "http://" : "https://") . $post_domain;
            $author->state         = "enabled";
            $author->level         = $user_level;
            $author->save();
            
            $author->set_engine_pref("@comment_likes:enable_notifications", "false");
            $author->set_engine_pref("@comments:email_on_post_comments",    "false");
            $author->set_engine_pref("@comments:email_on_comment_replies",  "false");
            $author->set_engine_pref("@contact:allow_emails",               "false");
            $author->set_engine_pref("@post_ratings:enable_notifications",  "false");
            $author->set_engine_pref("user_blocking.pms/*",                 "true" );
            
            $mem_cache->purge_by_prefix("accounts_repository/get_multiple_engine_prefs/");
            
            cli_colortags::write("<green> │   Author '$author->display_name' account created. Id: $author->id_account</green>\n");
        }
    }
    $post->id_author = $author->id_account;
}

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
    
    cli_colortags::write("<light_blue> │   Fetching main image from source...</light_blue>\n");
    
    if( ! in_array($extension, array("png", "jpg", "jpeg", "gif")) )
    {
        cli_colortags::write("<yellow> │   Warning: $filename is not a valid image! Main image wont be set.</yellow>\n");
        
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
        cli_colortags::write("<yellow> │   Warning: can't fetch image from {$url}: $error. Image ignored.</yellow>\n");
        
        return null;
    }
    
    curl_close($ch);
    
    if( empty($contents) )
    {
        cli_colortags::write("<yellow> │   Warning: image from {$url} is empty. Image ignored.</yellow>\n");
        
        return null;
    }
    
    if( ! @file_put_contents($file["tmp_name"], $contents) )
    {
        cli_colortags::write("<yellow> │   Warning: cannot save image {$file["tmp_name"]}. Image ignored.</yellow>\n");
        
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
        cli_colortags::write("<yellow> │   Warning: cannot save media item:</yellow>\n");
        cli_colortags::write(" |   " . str_replace("\n", "\n |   ", trim($res)) . "\n");
        cli_colortags::write("<yellow> |   Image ignored.</yellow>\n");
        
        return null;
    }
    
    cli_colortags::write("<light_blue> │   Done. Image id set to $res->id_media.</light_blue>\n");
    
    return $res;
}
