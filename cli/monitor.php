<?php
/**
 * CLI coin daemon monitor
 * Requires cron job to be set every 4 hours:
 * 
 * @package    vsimporter
 * @author     Alejandro Caballero - lava.caballero@gmail.com
 */

use hng2_base\account;
use hng2_media\media_repository;
use hng2_modules\categories\categories_repository;
use hng2_modules\categories\category_record;
use hng2_modules\posts\post_record;
use hng2_modules\posts\posts_repository;
use hng2_tools\cli_colortags;

chdir(__DIR__);

include "../../config.php";
include "../../includes/bootstrap.inc";
include "../../includes/self_running_checker.inc";
include "functions.inc";

#region Prechecks

if( isset($_SERVER["HTTP_HOST"]) ) die("This script cannot be called over the web.");

$now = date("Y-m-d H:i:s");

define("LOCK_FILE", "{$config->datafiles_location}/vsimporter.pid");
if( ! is_writable($config->datafiles_location) )
{
    cli_colortags::write(
        "<red>[$now] Critical: cannot create lockfile on</red> <light_red>$config->datafiles_location</light_red>!\n" .
        "Please make sure that {$config->datafiles_location} is writable. If not, chmod it to 777. " .
        "<red>Aborting.</red>\n\n"
    );
    
    die();
}

$feed_url = $settings->get("modules:vsimporter.feed_url");

if( empty($feed_url) )
{
    cli_colortags::write(
        "<red>[$now] Critical: feed URL unset</red>\n" .
        "<red>Please open the settings editor and define the VS feed URL.</red>\n" .
        "<red>Aborting.</red>\n\n"
    );
    
    die();
}

$item_url_prefix = $settings->get("modules:vsimporter.item_url_prefix");

if( empty($item_url_prefix) )
{
    cli_colortags::write(
        "<red>[$now] Critical: item URL prefix unset</red>\n" .
        "<red>Please open the settings editor and define the VS item URL prefix.</red>\n" .
        "<red>Aborting.</red>\n\n"
    );
    
    die();
}

$raw_categories = $settings->get("modules:vsimporter.categories");

if( empty($raw_categories) )
{
    cli_colortags::write(
        "<red>[$now] Critical: categories and keywords are unset!</red>\n" .
        "<red>Please open the settings editor and define categories and keywords.</red>\n" .
        "<red>Aborting.</red>\n\n"
    );
    
    die();
}

if( self_running_checker() )
{
    cli_colortags::write("<red>\n[$now] Another instance is running. Aborting.</red>\n\n");
    
    die();
}

#endregion

$start = time();
cli_colortags::write("<light_cyan>[$now] - Starting run.</light_cyan>\n\n");

#--------------------------------------------------------------#
cli_colortags::write("<cyan>Checking categories...</cyan>\n");
#--------------------------------------------------------------#

$setting_categories = array();
$category_title      = "";
foreach(explode("\n", $raw_categories) as $line)
{
    $line = trim($line);
    
    if( empty($line) ) continue;
    if( substr($line, 0, 1) == "#" ) continue;
    
    if( stristr($line, ":") !== false )
    {
        # Category initiator
        
        $parts = explode(":", $line);
        $category_title = trim($parts[0]);
        $keywords         = preg_split('/\s*,\s*/', trim($parts[1]));
    }
    else
    {
        # Category continuator
        $keywords = preg_split('/\s*,\s*/', $line);
    }
    
    if( empty($keywords) ) continue;
    
    $keywords = array_unique($keywords);
    foreach($keywords as $key => $val) if( empty($val) ) unset($keywords[$key]);
    
    if( empty($category_title) )
    {
        cli_colortags::write(
            "<yellow>Warning: malformed categories list!</yellow>\n" .
            "<yellow>Please open the settings editor and make sure the categories and keywords are properly specified.</yellow>\n" .
            "<yellow>Aborting.</yellow>\n\n"
        );
        @unlink(LOCK_FILE);
        
        die();
    }
    
    if( ! isset($setting_categories[$category_title]) )
        $setting_categories[$category_title] = $keywords;
    else
        $setting_categories[$category_title] = array_merge($setting_categories[$category_title], $keywords);
}

$categories_repository = new categories_repository();
$all_categories        = $categories_repository->find(array(), 0, 0, "title asc");

/** @var category_record[] $existing_categories */
$existing_categories = array();
foreach($all_categories as $category) $existing_categories[$category->title] = $category;

$category_ids_by_title = array();
foreach($setting_categories as $title => $keywords)
{
    if( isset($existing_categories[$title]) )
    {
        $category_ids_by_title[$title] = $existing_categories[$title]->id_category;
    }
    else
    {
        $category = new category_record(array(
            "slug"       => wp_sanitize_filename($title),
            "title"      => $title,
            "visibility" => "public",
        ));
        $category->set_new_id();
        $categories_repository->save($category);
        
        $category_ids_by_title[$title] = $category->id_category;
        $existing_categories[$title]   = $category;
        
        cli_colortags::write("<green> > Category '$title' ($category->id_category) created.</green>\n");
    }
}
cli_colortags::write("<cyan>Categories check finished.</cyan>\n");

#-----------------------------------------------------------------#
cli_colortags::write("<cyan>Starting feed integration</cyan>\n");
#-----------------------------------------------------------------#

cli_colortags::write("<light_gray> > Fetching feed...</light_gray> ");

$ch = curl_init();
curl_setopt( $ch, CURLOPT_URL,            $feed_url);
curl_setopt( $ch, CURLOPT_RETURNTRANSFER, true     );
curl_setopt( $ch, CURLOPT_CONNECTTIMEOUT, 5        );
curl_setopt( $ch, CURLOPT_TIMEOUT,        10       );
curl_setopt( $ch, CURLOPT_FOLLOWLOCATION, true     );
$t   = time();
$res = curl_exec($ch);
if( curl_error($ch) )
{
    $error = curl_error($ch);
    
    cli_colortags::write(
        "\n<yellow>Error fetching the feed:</yellow>\n" .
        "<yellow>$error</yellow>\n" .
        "<yellow>Aborting.</yellow>\n\n"
    );
    @unlink(LOCK_FILE);
    
    die();
}
cli_colortags::write(sprintf(
    "<light_green>OK!</light_green> <white>%s KiB downloaded in %s seconds.</white>\n",
    number_format(strlen($res) / 1024, 2),
    time() - $t
));
curl_close($ch);

$feed = json_decode($res);
if( empty($feed) )
{
    cli_colortags::write(
        "<yellow>Feed malformed!</yellow>\n" .
        "<yellow>The feed contents doesn't seem to be JSON encoded.</yellow>\n" .
        "<yellow>Please check the URL and fetch it manually, then validate it.</yellow>\n" .
        "<yellow>Aborting.</yellow>\n\n"
    );
    @unlink(LOCK_FILE);
    
    die();
}

if( empty($feed->pages) )
{
    cli_colortags::write("<light_purple> > Feed seems to be empty.</light_purple>\n");
    cli_colortags::write("<cyan>Feed integration finished.</cyan>\n");
    $seconds = time() - $start;
    cli_colortags::write("<cyan>Finished in $seconds seconds.</cyan>\n\n");
    @unlink(LOCK_FILE);
    
    die();
}

$feed->pages = (array) $feed->pages;

$posts_repository = new posts_repository();
$media_repository = new media_repository();

$user_level = $settings->get("modules:vsimporter.user_level");
if( empty($user_level) ) $user_level = 100;

$fallback_account = $settings->get("modules:vsimporter.default_account_id");
if( empty($fallback_account) ) $fallback_account = 100000000000000;

$excerpt_length = $settings->get("modules:posts.excerpt_length");
if( empty($excerpt_length) ) $excerpt_length = 30;

$default_category_id = $settings->get("modules:vsimporter.default_category_id");
if( empty($default_category_id) ) $default_category_id = "0000000000000";

$index = 0;
$count = count($feed->pages);
cli_colortags::write("<light_gray> ┌ Starting loop on $count pages...</light_gray>\n");
foreach($feed->pages as $page)
{
    $index++;
    
    # Pre-forging
    $post = new post_record(array(
        "id_post"         => "420" . sprintf("%09.0f", $page->id),
        "slug"            => wp_sanitize_filename($page->title),
        "title"           => ucwords($page->title),
        "excerpt"         => make_excerpt_of($page->microdata->summary, $excerpt_length),
        "visibility"      => "public",
        "status"          => "published",
        "allow_comments"  => "1",
        "creation_date"   => date("Y-m-d H:i:s", strtotime($page->microdata->date)),
        "publishing_date" => date("Y-m-d H:i:s", strtotime($page->microdata->date)),
        "last_udpate"     => date("Y-m-d H:i:s"),
    ));
    
    # Check for existence by id
    $tmp = $posts_repository->get($post->id_post);
    if( ! is_null($tmp) )
    {
        # cli_colortags::write("<yellow> │ ($index/$count) Post #$post->id_post '$post->title' exists - skipped.</yellow>\n");
        
        continue;
    }
    
    # Notify
    cli_colortags::write("<green> │ > ($index/$count) Got #$post->id_post '$post->title'.</green>\n");
    
    # Fetch article
    $article_url = $item_url_prefix . urlencode($page->url);
    cli_colortags::write("<light_gray> │ > Fetching article...</light_gray> ");
    $ch = curl_init();
    curl_setopt( $ch, CURLOPT_URL,            $article_url);
    curl_setopt( $ch, CURLOPT_RETURNTRANSFER, true     );
    curl_setopt( $ch, CURLOPT_CONNECTTIMEOUT, 5        );
    curl_setopt( $ch, CURLOPT_TIMEOUT,        10       );
    curl_setopt( $ch, CURLOPT_FOLLOWLOCATION, true     );
    $t   = time();
    $res = curl_exec($ch);
    if( curl_error($ch) )
    {
        $error = curl_error($ch);
        cli_colortags::write(
            "\n" .
            "<yellow> │   Error fetching the article!</yellow>\n" .
            "<yellow> │   $error</yellow>\n" .
            "<yellow> │   Article skipped.</yellow>\n"
        );
        
        continue;
    }
    cli_colortags::write(sprintf(
        "<light_green>OK!</light_green> <white>%s KiB downloaded in %s seconds.</white>\n",
        number_format(strlen($res) / 1024, 2),
        time() - $t
    ));
    curl_close($ch);
    $obj = json_decode($res);
    if( empty($obj) )
    {
        cli_colortags::write(
            "<yellow> │   Article malformed! It seems to be an invalid JSON object. Skipping it</yellow>\n"
        );
        
        continue;
    }
    if( empty($obj->pages) )
    {
        cli_colortags::write(
            "<yellow> │   Article doesn't seem to have contents. Skipping it.</yellow>\n"
        );
        
        continue;
    }
    $article    = current($obj->pages);
    $paragraphs = explode("\n\n", $article->summary);
    $raw_conent = "$post->title\n\n";
    foreach($paragraphs as $paragraph)
    {
        $paragraph = trim($paragraph);
        
        if( empty($paragraph) ) continue;
        if( $paragraph == "." ) continue;
        
        $paragraph      = preg_replace('/ \./', "", $paragraph);
        $raw_conent    .= "$paragraph\n\n";
        $post->content .= "<p>$paragraph</p>\n";
    }
    
    # Scan categories
    $tags       = array();
    $hits       = array();
    $matches    = array();
    $raw_conent = strtolower($raw_conent);
    foreach($setting_categories as $title => $keywords)
    {
        foreach($keywords as $keyword)
        {
            $keyword = strtolower($keyword);
            $kcount  = substr_count($raw_conent, $keyword);
            
            if( $kcount == 0 ) continue;
            
            $matches[$title][$keyword] = $kcount;
            $hits[$title]++;
        }
    }
    if( empty($matches) )
    {
        cli_colortags::write(
            "<yellow> │   Article didn't hit any category. Setting it to fallback.</yellow>\n"
        );
        $post->main_category = $default_category_id;
    }
    else
    {
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
            "<light_gray> │ > Post category will be set to </light_gray><light_blue>$winner_category</light_blue> " .
            "<light_gray>(#$category_id). Score: </light_gray><light_blue>$score</light_blue>.</light_gray>\n"
        );
        $post->main_category = $category_id;
        
        # Adding hashtags
        foreach($matches[$winner_category] as $keyword => $kcount) $tags[] = str_replace(array(" ", "-"), "", ucwords($keyword));
        
        $post->content .= "<p>#" . implode(" #", $tags) . "</p>";
    }
    
    # Reforge slug if necessary
    $tmp = $posts_repository->get_record_count(array("slug like '{$post->slug}%'"));
    if( $tmp > 0 ) $post->slug .= $tmp;
    
    # Forge author
    $author_id   = $fallback_account;
    $post_domain = current(explode("/", preg_replace('#http://|https://|www\.#i', "", $page->url)));
    if( empty($post_domain) )
    {
        cli_colortags::write("<yellow> │ > Couldn't extract domain for author check! Setting author to fallback account id.</yellow>\n");
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
                cli_colortags::write("<yellow> │ > Author account found, but it doesn't have the proper level!</yellow>\n");
                cli_colortags::write("<yellow> │   Will be set to fallback account id.</yellow>\n");
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
            
            cli_colortags::write("<green> │ > Author '$author->display_name' account created. Id: $author->id_account</green>\n");
        }
    }
    $post->id_author = $author->id_account;
    
    # Source addition
    $post->content .= "<p><i>Source: <a href='$page->url' target='_blank'>$page->url</a></i></p>\n";
    
    # Featured image
    if( ! empty($page->main_image_url) )
    {
        $item = get_featured_image($page->main_image_url, $page->url);
        if( ! empty($item) )
        {
            $post->id_featured_image = $item->id_media;
            
            $post->content = sprintf(
                    '<p class="aligncenter"><img src="%s" data-id-media="%s" data-media-type="image"></p>\n',
                    $item->get_item_url(),
                    $item->id_media
                ) . $post->content;
        }
    }
    
    $posts_repository->save($post);
    if( ! empty($tags) ) sort($tags);
    if( ! empty($tags) ) $posts_repository->set_tags($tags, $post->id_post);
    if( ! empty($post->id_featured_image) ) $posts_repository->set_media_items(array($post->id_featured_image), $post->id_post);
    
    cli_colortags::write("<green> │ > Post saved.</green>");
    if( ! empty($tags) ) cli_colortags::write("<light_green> Tags: #" . implode(" #", $tags) . "</light_green>");
    cli_colortags::write("\n");
}
cli_colortags::write("<light_gray> └ Loop finished.</light_gray>\n");



cli_colortags::write("<cyan>Feed integration finished.</cyan>\n");

$seconds = time() - $start;
cli_colortags::write("<cyan>Finished in $seconds seconds.</cyan>\n\n");
@unlink(LOCK_FILE);