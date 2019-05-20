<?php
/**
 * Settings field type extenders: feeds forger.
 * Called from within a function.
 *
 * @package    BardCanvas
 * @subpackage gallery
 * @author     Alejandro Caballero - lava.caballero@gmail.com
 *
 * Trailing globals:
 * @var module[] $modules
 * @var module   $current_module settings
 * @var module   $this_module    self
 */

use hng2_base\module;

global $config;

/** @var array $params [name => string, value => string, options => SimpleXMLElement] */
$params = $config->globals["specs_renderer/feeds_forger"];
?>

<script type="text/javascript">
    
    function add_vsi_feed(name, url, prefix)
    {
        var $template = $( $('#vsi_feed_template').html() );
        
        if( typeof name   !== 'undefined' ) $template.find('.vsi_feed_name').val( name );
        if( typeof url    !== 'undefined' ) $template.find('.vsi_feed_url').val( url );
        if( typeof prefix !== 'undefined' ) $template.find('.vsi_feed_prefix').val( prefix );
        
        $('#vsi_feeds_forger').find('.elements').append( $template );
    }
    
    function forge_vsi_feeds_value()
    {
        var $container = $('#vsi_feeds_forger');
        var markup     = '';
        
        $container.find('.elements .vsi_feed_element').each(function()
        {
            var $this = $(this);
            
            markup = markup + sprintf(
                '%s\t%s\t%s\n',
                $this.find('.vsi_feed_name').val().trim(),
                $this.find('.vsi_feed_url').val().trim(),
                $this.find('.vsi_feed_prefix').val().trim()
            )
        });
        
        $container.find('.vsi_feeds_value').val(markup);
    }
    
    $(document).ready(function()
    {
        var $container = $('#vsi_feeds_forger');
        var value      = $container.find('.vsi_feeds_value').val();
        
        if( value === '' ) return;
        
        var lines = value.split('\n');
        for(var i in lines)
        {
            var line  = lines[i];
            var parts = line.split('\t');
            add_vsi_feed(parts[0], parts[1], parts[2]);
        }
    });
    
</script>

<div id="vsi_feeds_forger">
    <textarea class="vsi_feeds_value" style="display: none" name="names[<?= $params["name"] ?>]"><?= $params["value"] ?></textarea>
    
    <div class="elements"></div>
    
    <button onclick="add_vsi_feed(); return false;">
        <i class="fa fa-plus"></i>
        <?= $this_module->language->add_feed ?>
    </button>
</div>

<template id="vsi_feed_template">
    
    <div class="framed_content vsi_feed_element" style="margin: 0 0 10px 0;">
        <table class="nav_table_inner">
            <tr>
                <th nowrap><?= $this_module->language->feed_name ?></th>
                <td style="width: 100%">
                    <input type="text" class="vsi_feed_name" value="" 
                           placeholder="<?= $this_module->language->type_optional_name ?>"
                           onblur="forge_vsi_feeds_value()">
                </td>
            </tr>
            <tr>
                <th nowrap><?= $this_module->language->feed_url ?></th>
                <td style="width: 100%">
                    <input type="text" class="vsi_feed_url" value="" 
                           placeholder="<?= $this_module->language->type_feed_url ?>"
                           onblur="forge_vsi_feeds_value()">
                </td>
            </tr>
            <tr>
                <th nowrap><?= $this_module->language->single_article_prefix ?></th>
                <td style="width: 100%">
                    <input type="text" class="vsi_feed_prefix" value=""
                           placeholder="<?= $this_module->language->type_single_url ?>"
                           onblur="forge_vsi_feeds_value()">
                </td>
            </tr>
        </table>
    </div>
    
</template>