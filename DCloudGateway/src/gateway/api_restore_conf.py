  


<!DOCTYPE html>
<html>
  <head prefix="og: http://ogp.me/ns# fb: http://ogp.me/ns/fb# githubog: http://ogp.me/ns/fb/githubog#">
    <meta charset='utf-8'>
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <title>StorageAppliance/DCloudGateway/src/gateway/api_restore_conf.py at gateway · Delta-Cloud/StorageAppliance</title>
    <link rel="search" type="application/opensearchdescription+xml" href="/opensearch.xml" title="GitHub" />
    <link rel="fluid-icon" href="https://github.com/fluidicon.png" title="GitHub" />
    <link rel="shortcut icon" href="/favicon.ico" type="image/x-icon" />
    <link rel="apple-touch-icon-precomposed" sizes="57x57" href="apple-touch-icon-114.png" />
    <link rel="apple-touch-icon-precomposed" sizes="114x114" href="apple-touch-icon-114.png" />
    <link rel="apple-touch-icon-precomposed" sizes="72x72" href="apple-touch-icon-144.png" />
    <link rel="apple-touch-icon-precomposed" sizes="144x144" href="apple-touch-icon-144.png" />

    
    

    <meta content="authenticity_token" name="csrf-param" />
<meta content="ebL4Saoa8Z6sudimSMs2FbzR0x3Cxnh+/L5ttdoaLzY=" name="csrf-token" />

    <link href="https://a248.e.akamai.net/assets.github.com/assets/github-6fe621c30ad13ea5c67f602a5bed5e90862419a8.css" media="screen" rel="stylesheet" type="text/css" />
    <link href="https://a248.e.akamai.net/assets.github.com/assets/github2-bbb63b3fd6f11e65f563d57794a7ae33e8ddb4a3.css" media="screen" rel="stylesheet" type="text/css" />
    
    


    <script src="https://a248.e.akamai.net/assets.github.com/assets/frameworks-41bb9f3a8ce0b760dd1d2428c5727a0cec9bf9bd.js" type="text/javascript"></script>
    
    <script defer="defer" src="https://a248.e.akamai.net/assets.github.com/assets/github-c8d1a0512ce9ded7252b297b8dfe51d7294d29c4.js" type="text/javascript"></script>
    
    

      <link rel='permalink' href='/Delta-Cloud/StorageAppliance/blob/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway/src/gateway/api_restore_conf.py'>
    <meta property="og:title" content="StorageAppliance"/>
    <meta property="og:type" content="githubog:gitrepository"/>
    <meta property="og:url" content="https://github.com/Delta-Cloud/StorageAppliance"/>
    <meta property="og:image" content="https://a248.e.akamai.net/assets.github.com/images/gravatars/gravatar-140.png?1334862345"/>
    <meta property="og:site_name" content="GitHub"/>
    <meta property="og:description" content="Contribute to StorageAppliance development by creating an account on GitHub."/>

    <meta name="description" content="Contribute to StorageAppliance development by creating an account on GitHub." />

  <link href="https://github.com/Delta-Cloud/StorageAppliance/commits/gateway.atom?login=yenkuo&token=af793f1b2259a2d7d88ade7bfe51fd2b" rel="alternate" title="Recent Commits to StorageAppliance:gateway" type="application/atom+xml" />

  </head>


  <body class="logged_in page-blob linux vis-private env-production ">
    <div id="wrapper">

    
    
    

      <div id="header" class="true clearfix">
        <div class="container clearfix">
          <a class="site-logo" href="https://github.com/">
            <!--[if IE]>
            <img alt="GitHub" class="github-logo" src="https://a248.e.akamai.net/assets.github.com/images/modules/header/logov7.png?1334862346" />
            <img alt="GitHub" class="github-logo-hover" src="https://a248.e.akamai.net/assets.github.com/images/modules/header/logov7-hover.png?1334862346" />
            <![endif]-->
            <img alt="GitHub" class="github-logo-4x" height="30" src="https://a248.e.akamai.net/assets.github.com/images/modules/header/logov7@4x.png?1337118066" />
            <img alt="GitHub" class="github-logo-4x-hover" height="30" src="https://a248.e.akamai.net/assets.github.com/images/modules/header/logov7@4x-hover.png?1337118066" />
          </a>


              
    <div class="topsearch  ">
        <form accept-charset="UTF-8" action="/search" id="top_search_form" method="get">
  <a href="/search" class="advanced-search tooltipped downwards" title="Advanced Search"><span class="mini-icon mini-icon-advanced-search"></span></a>
  <div class="search placeholder-field js-placeholder-field">
    <input type="text" class="search my_repos_autocompleter" id="global-search-field" name="q" results="5" spellcheck="false" autocomplete="off" data-autocomplete="my-repos-autocomplete" placeholder="Search&hellip;" data-hotkey="s" />
    <div id="my-repos-autocomplete" class="autocomplete-results">
      <ul class="js-navigation-container"></ul>
    </div>
    <input type="submit" value="Search" class="button">
    <span class="mini-icon mini-icon-search-input"></span>
  </div>
  <input type="hidden" name="type" value="Everything" />
  <input type="hidden" name="repo" value="" />
  <input type="hidden" name="langOverride" value="" />
  <input type="hidden" name="start_value" value="1" />
</form>

      <ul class="top-nav">
          <li class="explore"><a href="https://github.com/explore">Explore</a></li>
          <li><a href="https://gist.github.com">Gist</a></li>
          <li><a href="/blog">Blog</a></li>
        <li><a href="http://help.github.com">Help</a></li>
      </ul>
    </div>


            


  <div id="userbox">
    <div id="user">
      <a href="https://github.com/yenkuo"><img height="20" src="https://secure.gravatar.com/avatar/a1cf781d062ffcf9432e40f1a3aa28f3?s=140&amp;d=https://a248.e.akamai.net/assets.github.com%2Fimages%2Fgravatars%2Fgravatar-140.png" width="20" /></a>
      <a href="https://github.com/yenkuo" class="name">yenkuo</a>
    </div>
    <ul id="user-links">
      <li>
        <a href="/new" id="new_repo" class="tooltipped downwards" title="Create a New Repo"><span class="mini-icon mini-icon-create"></span></a>
      </li>
      <li>
        <a href="/inbox/notifications" id="notifications" class="tooltipped downwards" title="Notifications">
          <span class="mini-icon mini-icon-notifications"></span>
          <span class="unread_count">1</span>
        </a>
      </li>
      <li>
        <a href="/settings/profile" id="account_settings"
          class="tooltipped downwards"
          title="Account Settings ">
          <span class="mini-icon mini-icon-account-settings"></span>
        </a>
      </li>
      <li>
          <a href="/logout" data-method="post" id="logout" class="tooltipped downwards" title="Sign Out">
            <span class="mini-icon mini-icon-logout"></span>
          </a>
      </li>
    </ul>
  </div>



          
        </div>
      </div>

      

            <div class="site hfeed" itemscope itemtype="http://schema.org/WebPage">
      <div class="container hentry">
        <div class="pagehead repohead instapaper_ignore readability-menu">
        <div class="title-actions-bar">
          



              <ul class="pagehead-actions">


          <li class="nspr">
            <a href="/Delta-Cloud/StorageAppliance/pull/new/gateway" class="minibutton btn-pull-request ">Pull Request</a>
          </li>


          <li class="js-toggler-container js-social-container watch-button-container on">
            <span class="watch-button"><a href="/Delta-Cloud/StorageAppliance/toggle_watch" class="minibutton btn-watch js-toggler-target lighter" data-remote="true" data-method="post" rel="nofollow">Watch</a><a class="social-count js-social-count" href="/Delta-Cloud/StorageAppliance/watchers">12</a></span>
            <span class="unwatch-button"><a href="/Delta-Cloud/StorageAppliance/toggle_watch" class="minibutton btn-unwatch js-toggler-target lighter" data-remote="true" data-method="post" rel="nofollow">Unwatch</a><a class="social-count js-social-count" href="/Delta-Cloud/StorageAppliance/watchers">12</a></span>
          </li>

              <li>
                <a href="/yenkuo/StorageAppliance" class="minibutton btn-fork js-toggler-target lighter" rel="nofollow">Your Fork</a><a href="/Delta-Cloud/StorageAppliance/network" class="social-count">12</a>
              </li>


    </ul>

          <h1 itemscope itemtype="http://data-vocabulary.org/Breadcrumb" class="entry-title private">
            <span class="repo-label"><span>private</span></span>
            <span class="mega-icon mega-icon-private-repo"></span>
            <span class="author vcard">
<a href="/Delta-Cloud" class="url fn" itemprop="url" rel="author">              <span itemprop="title">Delta-Cloud</span>
              </a></span> /
            <strong><a href="/Delta-Cloud/StorageAppliance" class="js-current-repository">StorageAppliance</a></strong>
          </h1>
        </div>

          

  <ul class="tabs">
    <li><a href="/Delta-Cloud/StorageAppliance" class="selected" highlight="repo_sourcerepo_downloadsrepo_commitsrepo_tagsrepo_branches">Code</a></li>
    <li><a href="/Delta-Cloud/StorageAppliance/network" highlight="repo_network">Network</a>
    <li><a href="/Delta-Cloud/StorageAppliance/pulls" highlight="repo_pulls">Pull Requests <span class='counter'>1</span></a></li>

      <li><a href="/Delta-Cloud/StorageAppliance/issues" highlight="repo_issues">Issues <span class='counter'>1</span></a></li>

      <li><a href="/Delta-Cloud/StorageAppliance/wiki" highlight="repo_wiki">Wiki</a></li>

    <li><a href="/Delta-Cloud/StorageAppliance/graphs" highlight="repo_graphsrepo_contributors">Graphs</a></li>

  </ul>
  
<div class="frame frame-center tree-finder" style="display:none"
      data-tree-list-url="/Delta-Cloud/StorageAppliance/tree-list/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267"
      data-blob-url-prefix="/Delta-Cloud/StorageAppliance/blob/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267"
    >

  <div class="breadcrumb">
    <span class="bold"><a href="/Delta-Cloud/StorageAppliance">StorageAppliance</a></span> /
    <input class="tree-finder-input js-navigation-enable" type="text" name="query" autocomplete="off" spellcheck="false">
  </div>

    <div class="octotip">
      <p>
        <a href="/Delta-Cloud/StorageAppliance/dismiss-tree-finder-help" class="dismiss js-dismiss-tree-list-help" title="Hide this notice forever" rel="nofollow">Dismiss</a>
        <span class="bold">Octotip:</span> You've activated the <em>file finder</em>
        by pressing <span class="kbd">t</span> Start typing to filter the
        file list. Use <span class="kbd badmono">↑</span> and
        <span class="kbd badmono">↓</span> to navigate,
        <span class="kbd">enter</span> to view files.
      </p>
    </div>

  <table class="tree-browser" cellpadding="0" cellspacing="0">
    <tr class="js-header"><th>&nbsp;</th><th>name</th></tr>
    <tr class="js-no-results no-results" style="display: none">
      <th colspan="2">No matching files</th>
    </tr>
    <tbody class="js-results-list js-navigation-container">
    </tbody>
  </table>
</div>

<div id="jump-to-line" style="display:none">
  <h2>Jump to Line</h2>
  <form accept-charset="UTF-8">
    <input class="textfield" type="text">
    <div class="full-button">
      <button type="submit" class="classy">
        Go
      </button>
    </div>
  </form>
</div>


<div class="subnav-bar">

  <ul class="actions subnav">
    <li><a href="/Delta-Cloud/StorageAppliance/tags" class="" highlight="repo_tags">Tags <span class="counter">8</span></a></li>
    <li><a href="/Delta-Cloud/StorageAppliance/downloads" class="blank downloads-blank" highlight="repo_downloads">Downloads <span class="counter">0</span></a></li>
    
  <li class="search repo-search ">
<form accept-charset="UTF-8" action="/Delta-Cloud/StorageAppliance/search" method="get">      <span class="fieldwrap">
        <input type="text" name="q" value=""
          placeholder="Search source code&hellip;" /><button type="submit" class="minibutton"><span class="mini-icon mini-icon-search-input"></span></button>
      </span>
      <input type="hidden" id="lang-value" name="langOverride" value="" />
      <input type="hidden" id="start-value" name="start" value="" />
</form>  </li>

  </ul>

  <ul class="scope">
    <li class="switcher">

      <div class="context-menu-container js-menu-container js-context-menu">
        <a href="#"
           class="minibutton bigger switcher js-menu-target js-commitish-button btn-branch repo-tree"
           data-hotkey="w"
           data-master-branch="gateway"
           data-ref="gateway">
           <span><i>branch:</i> gateway</span>
        </a>

        <div class="context-pane commitish-context js-menu-content">
          <a href="javascript:;" class="close js-menu-close"><span class="mini-icon mini-icon-remove-close"></span></a>
          <div class="context-title">Switch branches/tags</div>
          <div class="context-body pane-selector commitish-selector js-navigation-container">
            <div class="filterbar">
              <input type="text" id="context-commitish-filter-field" class="js-navigation-enable" placeholder="Filter branches/tags" data-filterable />

              <ul class="tabs">
                <li><a href="#" data-filter="branches" class="selected">Branches</a></li>
                <li><a href="#" data-filter="tags">Tags</a></li>
              </ul>
            </div>

            <div class="js-filter-tab js-filter-branches" data-filterable-for="context-commitish-filter-field" data-filterable-type=substring>
              <div class="no-results js-not-filterable">Nothing to show</div>
                <div class="commitish-item branch-commitish selector-item js-navigation-item js-navigation-target selected">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/gateway/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="gateway" rel="nofollow">gateway</a>
                  </h4>
                </div>
                <div class="commitish-item branch-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/master/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="master" rel="nofollow">master</a>
                  </h4>
                </div>
                <div class="commitish-item branch-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/release/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="release" rel="nofollow">release</a>
                  </h4>
                </div>
                <div class="commitish-item branch-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/storage_ui/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="storage_ui" rel="nofollow">storage_ui</a>
                  </h4>
                </div>
            </div>

            <div class="js-filter-tab js-filter-tags" style="display:none" data-filterable-for="context-commitish-filter-field" data-filterable-type=substring>
              <div class="no-results js-not-filterable">Nothing to show</div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.7/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.7" rel="nofollow">v1.0.7</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.6/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.6" rel="nofollow">v1.0.6</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.5/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.5" rel="nofollow">v1.0.5</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.4/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.4" rel="nofollow">v1.0.4</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.3/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.3" rel="nofollow">v1.0.3</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.2/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.2" rel="nofollow">v1.0.2</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v1.0.1/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v1.0.1" rel="nofollow">v1.0.1</a>
                  </h4>
                </div>
                <div class="commitish-item tag-commitish selector-item js-navigation-item js-navigation-target ">
                  <span class="mini-icon mini-icon-confirm"></span>
                  <h4>
                      <a href="/Delta-Cloud/StorageAppliance/blob/v0.1/DCloudGateway/src/gateway/api_restore_conf.py" class="js-navigation-open" data-name="v0.1" rel="nofollow">v0.1</a>
                  </h4>
                </div>
            </div>
          </div>
        </div><!-- /.commitish-context-context -->
      </div>

    </li>
  </ul>

  <ul class="subnav with-scope">

    <li><a href="/Delta-Cloud/StorageAppliance" class="selected" highlight="repo_source">Files</a></li>
    <li><a href="/Delta-Cloud/StorageAppliance/commits/gateway" highlight="repo_commits">Commits</a></li>
    <li><a href="/Delta-Cloud/StorageAppliance/branches" class="" highlight="repo_branches" rel="nofollow">Branches <span class="counter">4</span></a></li>
  </ul>

</div>

  
  
  


          

        </div><!-- /.repohead -->

        <div id="js-repo-pjax-container" data-pjax-container>
          




<!-- blob contrib key: blob_contributors:v21:082db999fe42ab622de815bbe7ec8ce2 -->
<!-- blob contrib frag key: views10/v8/blob_contributors:v21:082db999fe42ab622de815bbe7ec8ce2 -->

<!-- block_view_fragment_key: views10/v8/blob:v21:316c81f3b0fa7def630b8c5a78b56e25 -->
  <div id="slider">

    <div class="breadcrumb" data-path="DCloudGateway/src/gateway/api_restore_conf.py/">
      <b itemscope="" itemtype="http://data-vocabulary.org/Breadcrumb"><a href="/Delta-Cloud/StorageAppliance/tree/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267" class="js-rewrite-sha" itemprop="url"><span itemprop="title">StorageAppliance</span></a></b> / <span itemscope="" itemtype="http://data-vocabulary.org/Breadcrumb"><a href="/Delta-Cloud/StorageAppliance/tree/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway" class="js-rewrite-sha" itemscope="url"><span itemprop="title">DCloudGateway</span></a></span> / <span itemscope="" itemtype="http://data-vocabulary.org/Breadcrumb"><a href="/Delta-Cloud/StorageAppliance/tree/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway/src" class="js-rewrite-sha" itemscope="url"><span itemprop="title">src</span></a></span> / <span itemscope="" itemtype="http://data-vocabulary.org/Breadcrumb"><a href="/Delta-Cloud/StorageAppliance/tree/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway/src/gateway" class="js-rewrite-sha" itemscope="url"><span itemprop="title">gateway</span></a></span> / <strong class="final-path">api_restore_conf.py</strong> <span class="js-clippy mini-icon mini-icon-clippy " data-clipboard-text="DCloudGateway/src/gateway/api_restore_conf.py" data-copied-hint="copied!" data-copy-hint="copy to clipboard"></span>
    </div>

      <div class="commit file-history-tease js-blob-contributors-loader" data-request-url="/Delta-Cloud/StorageAppliance/contributors/gateway/DCloudGateway/src/gateway/api_restore_conf.py" data-path="DCloudGateway/src/gateway/api_restore_conf.py/">
        Fetching contributors…

        <div class="participation">
          <p class="quickstat js-loading-status-text"><img alt="Octocat-spinner-16px" height="16" src="https://a248.e.akamai.net/assets.github.com/images/spinners/octocat-spinner-16px.gif?1334862346" width="16" /></p>
        </div>
      </div>

    <div class="frames">
      <div class="frame frame-center" data-path="DCloudGateway/src/gateway/api_restore_conf.py/" data-permalink-url="/Delta-Cloud/StorageAppliance/blob/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway/src/gateway/api_restore_conf.py" data-title="StorageAppliance/DCloudGateway/src/gateway/api_restore_conf.py at gateway · Delta-Cloud/StorageAppliance · GitHub" data-type="blob">

        <div id="files" class="bubble">
          <div class="file">
            <div class="meta">
              <div class="info">
                <span class="icon"><b class="mini-icon mini-icon-text-file"></b></span>
                <span class="mode" title="File Mode">file</span>
                  <span>220 lines (189 sloc)</span>
                <span>7.347 kb</span>
              </div>
              <ul class="button-group actions">
                  <li>
                    <a class="grouped-button file-edit-link minibutton bigger lighter js-rewrite-sha" href="/Delta-Cloud/StorageAppliance/edit/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267/DCloudGateway/src/gateway/api_restore_conf.py" data-method="post" rel="nofollow" data-hotkey="e">Edit</a>
                  </li>

                <li>
                  <a href="/Delta-Cloud/StorageAppliance/raw/gateway/DCloudGateway/src/gateway/api_restore_conf.py" class="minibutton btn-raw grouped-button bigger lighter" id="raw-url">Raw</a>
                </li>
                  <li>
                    <a href="/Delta-Cloud/StorageAppliance/blame/gateway/DCloudGateway/src/gateway/api_restore_conf.py" class="minibutton btn-blame grouped-button bigger lighter">Blame</a>
                  </li>
                <li>
                  <a href="/Delta-Cloud/StorageAppliance/commits/gateway/DCloudGateway/src/gateway/api_restore_conf.py" class="minibutton btn-history grouped-button bigger lighter" rel="nofollow">History</a>
                </li>
              </ul>
            </div>
              <div class="data type-python">
      <table cellpadding="0" cellspacing="0" class="lines">
        <tr>
          <td>
            <pre class="line_numbers"><span id="L1" rel="#L1">1</span>
<span id="L2" rel="#L2">2</span>
<span id="L3" rel="#L3">3</span>
<span id="L4" rel="#L4">4</span>
<span id="L5" rel="#L5">5</span>
<span id="L6" rel="#L6">6</span>
<span id="L7" rel="#L7">7</span>
<span id="L8" rel="#L8">8</span>
<span id="L9" rel="#L9">9</span>
<span id="L10" rel="#L10">10</span>
<span id="L11" rel="#L11">11</span>
<span id="L12" rel="#L12">12</span>
<span id="L13" rel="#L13">13</span>
<span id="L14" rel="#L14">14</span>
<span id="L15" rel="#L15">15</span>
<span id="L16" rel="#L16">16</span>
<span id="L17" rel="#L17">17</span>
<span id="L18" rel="#L18">18</span>
<span id="L19" rel="#L19">19</span>
<span id="L20" rel="#L20">20</span>
<span id="L21" rel="#L21">21</span>
<span id="L22" rel="#L22">22</span>
<span id="L23" rel="#L23">23</span>
<span id="L24" rel="#L24">24</span>
<span id="L25" rel="#L25">25</span>
<span id="L26" rel="#L26">26</span>
<span id="L27" rel="#L27">27</span>
<span id="L28" rel="#L28">28</span>
<span id="L29" rel="#L29">29</span>
<span id="L30" rel="#L30">30</span>
<span id="L31" rel="#L31">31</span>
<span id="L32" rel="#L32">32</span>
<span id="L33" rel="#L33">33</span>
<span id="L34" rel="#L34">34</span>
<span id="L35" rel="#L35">35</span>
<span id="L36" rel="#L36">36</span>
<span id="L37" rel="#L37">37</span>
<span id="L38" rel="#L38">38</span>
<span id="L39" rel="#L39">39</span>
<span id="L40" rel="#L40">40</span>
<span id="L41" rel="#L41">41</span>
<span id="L42" rel="#L42">42</span>
<span id="L43" rel="#L43">43</span>
<span id="L44" rel="#L44">44</span>
<span id="L45" rel="#L45">45</span>
<span id="L46" rel="#L46">46</span>
<span id="L47" rel="#L47">47</span>
<span id="L48" rel="#L48">48</span>
<span id="L49" rel="#L49">49</span>
<span id="L50" rel="#L50">50</span>
<span id="L51" rel="#L51">51</span>
<span id="L52" rel="#L52">52</span>
<span id="L53" rel="#L53">53</span>
<span id="L54" rel="#L54">54</span>
<span id="L55" rel="#L55">55</span>
<span id="L56" rel="#L56">56</span>
<span id="L57" rel="#L57">57</span>
<span id="L58" rel="#L58">58</span>
<span id="L59" rel="#L59">59</span>
<span id="L60" rel="#L60">60</span>
<span id="L61" rel="#L61">61</span>
<span id="L62" rel="#L62">62</span>
<span id="L63" rel="#L63">63</span>
<span id="L64" rel="#L64">64</span>
<span id="L65" rel="#L65">65</span>
<span id="L66" rel="#L66">66</span>
<span id="L67" rel="#L67">67</span>
<span id="L68" rel="#L68">68</span>
<span id="L69" rel="#L69">69</span>
<span id="L70" rel="#L70">70</span>
<span id="L71" rel="#L71">71</span>
<span id="L72" rel="#L72">72</span>
<span id="L73" rel="#L73">73</span>
<span id="L74" rel="#L74">74</span>
<span id="L75" rel="#L75">75</span>
<span id="L76" rel="#L76">76</span>
<span id="L77" rel="#L77">77</span>
<span id="L78" rel="#L78">78</span>
<span id="L79" rel="#L79">79</span>
<span id="L80" rel="#L80">80</span>
<span id="L81" rel="#L81">81</span>
<span id="L82" rel="#L82">82</span>
<span id="L83" rel="#L83">83</span>
<span id="L84" rel="#L84">84</span>
<span id="L85" rel="#L85">85</span>
<span id="L86" rel="#L86">86</span>
<span id="L87" rel="#L87">87</span>
<span id="L88" rel="#L88">88</span>
<span id="L89" rel="#L89">89</span>
<span id="L90" rel="#L90">90</span>
<span id="L91" rel="#L91">91</span>
<span id="L92" rel="#L92">92</span>
<span id="L93" rel="#L93">93</span>
<span id="L94" rel="#L94">94</span>
<span id="L95" rel="#L95">95</span>
<span id="L96" rel="#L96">96</span>
<span id="L97" rel="#L97">97</span>
<span id="L98" rel="#L98">98</span>
<span id="L99" rel="#L99">99</span>
<span id="L100" rel="#L100">100</span>
<span id="L101" rel="#L101">101</span>
<span id="L102" rel="#L102">102</span>
<span id="L103" rel="#L103">103</span>
<span id="L104" rel="#L104">104</span>
<span id="L105" rel="#L105">105</span>
<span id="L106" rel="#L106">106</span>
<span id="L107" rel="#L107">107</span>
<span id="L108" rel="#L108">108</span>
<span id="L109" rel="#L109">109</span>
<span id="L110" rel="#L110">110</span>
<span id="L111" rel="#L111">111</span>
<span id="L112" rel="#L112">112</span>
<span id="L113" rel="#L113">113</span>
<span id="L114" rel="#L114">114</span>
<span id="L115" rel="#L115">115</span>
<span id="L116" rel="#L116">116</span>
<span id="L117" rel="#L117">117</span>
<span id="L118" rel="#L118">118</span>
<span id="L119" rel="#L119">119</span>
<span id="L120" rel="#L120">120</span>
<span id="L121" rel="#L121">121</span>
<span id="L122" rel="#L122">122</span>
<span id="L123" rel="#L123">123</span>
<span id="L124" rel="#L124">124</span>
<span id="L125" rel="#L125">125</span>
<span id="L126" rel="#L126">126</span>
<span id="L127" rel="#L127">127</span>
<span id="L128" rel="#L128">128</span>
<span id="L129" rel="#L129">129</span>
<span id="L130" rel="#L130">130</span>
<span id="L131" rel="#L131">131</span>
<span id="L132" rel="#L132">132</span>
<span id="L133" rel="#L133">133</span>
<span id="L134" rel="#L134">134</span>
<span id="L135" rel="#L135">135</span>
<span id="L136" rel="#L136">136</span>
<span id="L137" rel="#L137">137</span>
<span id="L138" rel="#L138">138</span>
<span id="L139" rel="#L139">139</span>
<span id="L140" rel="#L140">140</span>
<span id="L141" rel="#L141">141</span>
<span id="L142" rel="#L142">142</span>
<span id="L143" rel="#L143">143</span>
<span id="L144" rel="#L144">144</span>
<span id="L145" rel="#L145">145</span>
<span id="L146" rel="#L146">146</span>
<span id="L147" rel="#L147">147</span>
<span id="L148" rel="#L148">148</span>
<span id="L149" rel="#L149">149</span>
<span id="L150" rel="#L150">150</span>
<span id="L151" rel="#L151">151</span>
<span id="L152" rel="#L152">152</span>
<span id="L153" rel="#L153">153</span>
<span id="L154" rel="#L154">154</span>
<span id="L155" rel="#L155">155</span>
<span id="L156" rel="#L156">156</span>
<span id="L157" rel="#L157">157</span>
<span id="L158" rel="#L158">158</span>
<span id="L159" rel="#L159">159</span>
<span id="L160" rel="#L160">160</span>
<span id="L161" rel="#L161">161</span>
<span id="L162" rel="#L162">162</span>
<span id="L163" rel="#L163">163</span>
<span id="L164" rel="#L164">164</span>
<span id="L165" rel="#L165">165</span>
<span id="L166" rel="#L166">166</span>
<span id="L167" rel="#L167">167</span>
<span id="L168" rel="#L168">168</span>
<span id="L169" rel="#L169">169</span>
<span id="L170" rel="#L170">170</span>
<span id="L171" rel="#L171">171</span>
<span id="L172" rel="#L172">172</span>
<span id="L173" rel="#L173">173</span>
<span id="L174" rel="#L174">174</span>
<span id="L175" rel="#L175">175</span>
<span id="L176" rel="#L176">176</span>
<span id="L177" rel="#L177">177</span>
<span id="L178" rel="#L178">178</span>
<span id="L179" rel="#L179">179</span>
<span id="L180" rel="#L180">180</span>
<span id="L181" rel="#L181">181</span>
<span id="L182" rel="#L182">182</span>
<span id="L183" rel="#L183">183</span>
<span id="L184" rel="#L184">184</span>
<span id="L185" rel="#L185">185</span>
<span id="L186" rel="#L186">186</span>
<span id="L187" rel="#L187">187</span>
<span id="L188" rel="#L188">188</span>
<span id="L189" rel="#L189">189</span>
<span id="L190" rel="#L190">190</span>
<span id="L191" rel="#L191">191</span>
<span id="L192" rel="#L192">192</span>
<span id="L193" rel="#L193">193</span>
<span id="L194" rel="#L194">194</span>
<span id="L195" rel="#L195">195</span>
<span id="L196" rel="#L196">196</span>
<span id="L197" rel="#L197">197</span>
<span id="L198" rel="#L198">198</span>
<span id="L199" rel="#L199">199</span>
<span id="L200" rel="#L200">200</span>
<span id="L201" rel="#L201">201</span>
<span id="L202" rel="#L202">202</span>
<span id="L203" rel="#L203">203</span>
<span id="L204" rel="#L204">204</span>
<span id="L205" rel="#L205">205</span>
<span id="L206" rel="#L206">206</span>
<span id="L207" rel="#L207">207</span>
<span id="L208" rel="#L208">208</span>
<span id="L209" rel="#L209">209</span>
<span id="L210" rel="#L210">210</span>
<span id="L211" rel="#L211">211</span>
<span id="L212" rel="#L212">212</span>
<span id="L213" rel="#L213">213</span>
<span id="L214" rel="#L214">214</span>
<span id="L215" rel="#L215">215</span>
<span id="L216" rel="#L216">216</span>
<span id="L217" rel="#L217">217</span>
<span id="L218" rel="#L218">218</span>
<span id="L219" rel="#L219">219</span>
</pre>
          </td>
          <td width="100%">
                <div class="highlight"><pre><div class='line' id='LC1'><span class="c">#!/usr/bin/python</span></div><div class='line' id='LC2'><br/></div><div class='line' id='LC3'><span class="c"># Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.</span></div><div class='line' id='LC4'><span class="c"># Function: API function for controlling HTTP proxy</span></div><div class='line' id='LC5'><br/></div><div class='line' id='LC6'><span class="kn">import</span> <span class="nn">os</span></div><div class='line' id='LC7'><span class="kn">import</span> <span class="nn">json</span></div><div class='line' id='LC8'><span class="kn">import</span> <span class="nn">api</span></div><div class='line' id='LC9'><span class="kn">import</span> <span class="nn">common</span></div><div class='line' id='LC10'><span class="kn">import</span> <span class="nn">ConfigParser</span></div><div class='line' id='LC11'><span class="kn">import</span> <span class="nn">subprocess</span></div><div class='line' id='LC12'><span class="kn">from</span> <span class="nn">BackupToCloud</span> <span class="kn">import</span> <span class="o">*</span></div><div class='line' id='LC13'><br/></div><div class='line' id='LC14'><span class="n">log</span> <span class="o">=</span> <span class="n">common</span><span class="o">.</span><span class="n">getLogger</span><span class="p">(</span><span class="n">name</span><span class="o">=</span><span class="s">&quot;API&quot;</span><span class="p">,</span> <span class="n">conf</span><span class="o">=</span><span class="s">&quot;/etc/delta/Gateway.ini&quot;</span><span class="p">)</span></div><div class='line' id='LC15'><br/></div><div class='line' id='LC16'><br/></div><div class='line' id='LC17'><span class="k">def</span> <span class="nf">_get_Swift_credential</span><span class="p">():</span></div><div class='line' id='LC18'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="sd">&quot;&quot;&quot;</span></div><div class='line' id='LC19'><span class="sd">    get swift credential setting</span></div><div class='line' id='LC20'><span class="sd">    &quot;&quot;&quot;</span></div><div class='line' id='LC21'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">info</span><span class="p">(</span><span class="s">&quot;_get_Swift_credential start&quot;</span><span class="p">)</span></div><div class='line' id='LC22'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">url</span> <span class="o">=</span> <span class="bp">None</span></div><div class='line' id='LC23'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">login</span> <span class="o">=</span> <span class="bp">None</span></div><div class='line' id='LC24'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">password</span> <span class="o">=</span> <span class="bp">None</span></div><div class='line' id='LC25'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">try</span><span class="p">:</span></div><div class='line' id='LC26'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">config</span> <span class="o">=</span> <span class="n">ConfigParser</span><span class="o">.</span><span class="n">ConfigParser</span><span class="p">()</span></div><div class='line' id='LC27'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">with</span> <span class="nb">open</span><span class="p">(</span><span class="s">&#39;/root/.s3ql/authinfo2&#39;</span><span class="p">)</span> <span class="k">as</span> <span class="n">op_fh</span><span class="p">:</span></div><div class='line' id='LC28'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">config</span><span class="o">.</span><span class="n">readfp</span><span class="p">(</span><span class="n">op_fh</span><span class="p">)</span></div><div class='line' id='LC29'><br/></div><div class='line' id='LC30'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">section</span> <span class="o">=</span> <span class="s">&quot;CloudStorageGateway&quot;</span></div><div class='line' id='LC31'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">url</span> <span class="o">=</span> <span class="n">config</span><span class="o">.</span><span class="n">get</span><span class="p">(</span><span class="n">section</span><span class="p">,</span> <span class="s">&#39;storage-url&#39;</span><span class="p">)</span><span class="o">.</span><span class="n">replace</span><span class="p">(</span><span class="s">&quot;swift://&quot;</span><span class="p">,</span> <span class="s">&quot;&quot;</span><span class="p">)</span></div><div class='line' id='LC32'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">login</span> <span class="o">=</span> <span class="n">config</span><span class="o">.</span><span class="n">get</span><span class="p">(</span><span class="n">section</span><span class="p">,</span> <span class="s">&#39;backend-login&#39;</span><span class="p">)</span></div><div class='line' id='LC33'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">password</span> <span class="o">=</span> <span class="n">config</span><span class="o">.</span><span class="n">get</span><span class="p">(</span><span class="n">section</span><span class="p">,</span> <span class="s">&#39;backend-password&#39;</span><span class="p">)</span></div><div class='line' id='LC34'><br/></div><div class='line' id='LC35'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">except</span> <span class="ne">Exception</span> <span class="k">as</span> <span class="n">e</span><span class="p">:</span></div><div class='line' id='LC36'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">error</span><span class="p">(</span><span class="s">&quot;Failed to _get_Swift_credential for </span><span class="si">%s</span><span class="s">&quot;</span> <span class="o">%</span> <span class="nb">str</span><span class="p">(</span><span class="n">e</span><span class="p">))</span></div><div class='line' id='LC37'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">finally</span><span class="p">:</span></div><div class='line' id='LC38'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">info</span><span class="p">(</span><span class="s">&quot;_get_Swift_credential end&quot;</span><span class="p">)</span></div><div class='line' id='LC39'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="p">[</span><span class="n">url</span><span class="p">,</span> <span class="n">login</span><span class="p">,</span> <span class="n">password</span><span class="p">]</span></div><div class='line' id='LC40'><br/></div><div class='line' id='LC41'><br/></div><div class='line' id='LC42'><span class="k">def</span> <span class="nf">get_configuration_backup_info</span><span class="p">():</span></div><div class='line' id='LC43'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="sd">&quot;&quot;&quot;</span></div><div class='line' id='LC44'><span class="sd">    Get the information of latest backup configuration.</span></div><div class='line' id='LC45'><span class="sd">        1. Get connection info for Swift.</span></div><div class='line' id='LC46'><span class="sd">        2. Probe whether there is a config file in Swift.</span></div><div class='line' id='LC47'><span class="sd">        3. If yes, download it and get the last backup date and time</span></div><div class='line' id='LC48'><span class="sd">    &quot;&quot;&quot;</span></div><div class='line' id='LC49'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ log.info(&quot;[2] get_configuration_backup_info start&quot;)</span></div><div class='line' id='LC50'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backup_info</span> <span class="o">=</span> <span class="n">_get_latest_backup</span><span class="p">()</span></div><div class='line' id='LC51'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ Case 1. There is no container &quot;config&quot;</span></div><div class='line' id='LC52'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">if</span> <span class="n">backup_info</span> <span class="ow">is</span> <span class="bp">None</span><span class="p">:</span></div><div class='line' id='LC53'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_ok</span> <span class="o">=</span> <span class="bp">False</span></div><div class='line' id='LC54'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_data</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;backup_time&#39;</span><span class="p">:</span> <span class="bp">None</span><span class="p">}</span></div><div class='line' id='LC55'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_code</span> <span class="o">=</span> <span class="s">&quot;000&quot;</span></div><div class='line' id='LC56'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_msg</span> <span class="o">=</span> <span class="s">&quot;There is no [config] container at Swift.&quot;</span></div><div class='line' id='LC57'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">else</span><span class="p">:</span></div><div class='line' id='LC58'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backup_time</span> <span class="o">=</span> <span class="n">backup_info</span><span class="p">[</span><span class="s">&#39;datetime&#39;</span><span class="p">]</span></div><div class='line' id='LC59'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ print backup_time</span></div><div class='line' id='LC60'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_ok</span> <span class="o">=</span> <span class="bp">True</span></div><div class='line' id='LC61'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_data</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;backup_time&#39;</span><span class="p">:</span> <span class="n">backup_time</span><span class="p">}</span></div><div class='line' id='LC62'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_code</span> <span class="o">=</span> <span class="s">&quot;100&quot;</span></div><div class='line' id='LC63'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_msg</span> <span class="o">=</span> <span class="bp">None</span></div><div class='line' id='LC64'><br/></div><div class='line' id='LC65'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;result&#39;</span><span class="p">:</span> <span class="n">op_ok</span><span class="p">,</span></div><div class='line' id='LC66'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;data&#39;</span><span class="p">:</span>   <span class="n">op_data</span><span class="p">,</span></div><div class='line' id='LC67'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;code&#39;</span><span class="p">:</span>   <span class="n">op_code</span><span class="p">,</span></div><div class='line' id='LC68'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;msg&#39;</span><span class="p">:</span>    <span class="n">op_msg</span><span class="p">}</span></div><div class='line' id='LC69'><br/></div><div class='line' id='LC70'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ log.info(&quot;[2] get_configuration_backup_info end&quot;)</span></div><div class='line' id='LC71'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">json</span><span class="o">.</span><span class="n">dumps</span><span class="p">(</span><span class="n">return_val</span><span class="p">)</span></div><div class='line' id='LC72'><br/></div><div class='line' id='LC73'><br/></div><div class='line' id='LC74'><span class="c">#----------------------------------------------------------------------</span></div><div class='line' id='LC75'><span class="k">def</span> <span class="nf">_get_latest_backup</span><span class="p">():</span></div><div class='line' id='LC76'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="sd">&quot;&quot;&quot;</span></div><div class='line' id='LC77'><span class="sd">    get the latest backup config from swift</span></div><div class='line' id='LC78'><span class="sd">    &quot;&quot;&quot;</span></div><div class='line' id='LC79'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="p">[</span><span class="n">url</span><span class="p">,</span> <span class="n">login</span><span class="p">,</span> <span class="n">password</span><span class="p">]</span> <span class="o">=</span> <span class="n">_get_Swift_credential</span><span class="p">()</span></div><div class='line' id='LC80'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;swift -A https://</span><span class="si">%s</span><span class="s">/auth/v1.0 -U </span><span class="si">%s</span><span class="s"> -K </span><span class="si">%s</span><span class="s"> list config&quot;</span> <span class="o">%</span>\</div><div class='line' id='LC81'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="p">(</span><span class="n">url</span><span class="p">,</span> <span class="n">login</span><span class="p">,</span> <span class="n">password</span><span class="p">)</span></div><div class='line' id='LC82'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">po</span> <span class="o">=</span> <span class="n">subprocess</span><span class="o">.</span><span class="n">Popen</span><span class="p">(</span><span class="n">cmd</span><span class="p">,</span> <span class="n">shell</span><span class="o">=</span><span class="bp">True</span><span class="p">,</span> <span class="n">stdout</span><span class="o">=</span><span class="n">subprocess</span><span class="o">.</span><span class="n">PIPE</span><span class="p">,</span> \</div><div class='line' id='LC83'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">stderr</span><span class="o">=</span><span class="n">subprocess</span><span class="o">.</span><span class="n">STDOUT</span><span class="p">)</span></div><div class='line' id='LC84'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">res</span> <span class="o">=</span> <span class="n">po</span><span class="o">.</span><span class="n">stdout</span><span class="o">.</span><span class="n">readlines</span><span class="p">()</span></div><div class='line' id='LC85'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">po</span><span class="o">.</span><span class="n">wait</span><span class="p">()</span></div><div class='line' id='LC86'><br/></div><div class='line' id='LC87'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ Case 1. There is no container &quot;config&quot;</span></div><div class='line' id='LC88'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">if</span> <span class="s">&quot;not found&quot;</span> <span class="ow">in</span> <span class="n">res</span><span class="p">[</span><span class="mi">0</span><span class="p">]:</span></div><div class='line' id='LC89'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="bp">None</span></div><div class='line' id='LC90'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">else</span><span class="p">:</span></div><div class='line' id='LC91'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ Case 2. Get a list of files</span></div><div class='line' id='LC92'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">latest_dt</span> <span class="o">=</span> <span class="o">-</span><span class="mi">999</span></div><div class='line' id='LC93'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">for</span> <span class="n">fn</span> <span class="ow">in</span> <span class="n">res</span><span class="p">:</span>   <span class="c"># find latest backup</span></div><div class='line' id='LC94'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">dt</span> <span class="o">=</span> <span class="n">fn</span><span class="p">[</span><span class="mi">0</span><span class="p">:</span><span class="mi">10</span><span class="p">]</span></div><div class='line' id='LC95'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">if</span> <span class="n">dt</span> <span class="o">&gt;</span> <span class="n">latest_dt</span><span class="p">:</span></div><div class='line' id='LC96'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">latest_dt</span> <span class="o">=</span> <span class="n">dt</span></div><div class='line' id='LC97'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">fname</span> <span class="o">=</span> <span class="n">fn</span>      <span class="c"># fname is the file name of latest backup</span></div><div class='line' id='LC98'><br/></div><div class='line' id='LC99'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backup_info</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;datetime&#39;</span><span class="p">:</span> <span class="n">latest_dt</span><span class="p">,</span> <span class="s">&#39;fname&#39;</span><span class="p">:</span> <span class="n">fname</span><span class="p">}</span></div><div class='line' id='LC100'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">backup_info</span></div><div class='line' id='LC101'><br/></div><div class='line' id='LC102'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="bp">None</span></div><div class='line' id='LC103'><br/></div><div class='line' id='LC104'><br/></div><div class='line' id='LC105'><span class="c">#----------------------------------------------------------------------</span></div><div class='line' id='LC106'><span class="k">def</span> <span class="nf">save_gateway_configuration</span><span class="p">():</span></div><div class='line' id='LC107'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="sd">&quot;&quot;&quot;</span></div><div class='line' id='LC108'><span class="sd">    Save current configuration to Cloud.</span></div><div class='line' id='LC109'><span class="sd">    &quot;&quot;&quot;</span></div><div class='line' id='LC110'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;result&#39;</span><span class="p">:</span> <span class="bp">False</span><span class="p">,</span></div><div class='line' id='LC111'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;code&#39;</span><span class="p">:</span>   <span class="s">&#39;001&#39;</span><span class="p">,</span></div><div class='line' id='LC112'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;msg&#39;</span><span class="p">:</span>    <span class="s">&#39;config backup is fail!&#39;</span><span class="p">}</span></div><div class='line' id='LC113'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">fileList</span> <span class="o">=</span> <span class="p">[</span><span class="s">&#39;/etc/delta/network.info&#39;</span><span class="p">,</span></div><div class='line' id='LC114'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;/etc/delta/gw_schedule.conf&#39;</span><span class="p">,</span></div><div class='line' id='LC115'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;/etc/exports&#39;</span><span class="p">,</span></div><div class='line' id='LC116'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;/etc/samba/smb.conf&#39;</span><span class="p">,</span></div><div class='line' id='LC117'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="p">]</span></div><div class='line' id='LC118'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">swiftData</span> <span class="o">=</span> <span class="n">_get_Swift_credential</span><span class="p">()</span></div><div class='line' id='LC119'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">if</span> <span class="n">swiftData</span><span class="p">[</span><span class="mi">0</span><span class="p">]</span> <span class="ow">is</span> <span class="bp">None</span><span class="p">:</span></div><div class='line' id='LC120'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span><span class="p">[</span><span class="s">&#39;code&#39;</span><span class="p">]</span> <span class="o">=</span> <span class="s">&#39;000&#39;</span></div><div class='line' id='LC121'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span><span class="p">[</span><span class="s">&#39;msg&#39;</span><span class="p">]</span> <span class="o">=</span> <span class="s">&#39;There is no [config] container at Swift.&#39;</span></div><div class='line' id='LC122'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">json</span><span class="o">.</span><span class="n">dumps</span><span class="p">(</span><span class="n">return_val</span><span class="p">)</span></div><div class='line' id='LC123'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">try</span><span class="p">:</span></div><div class='line' id='LC124'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">swiftObj</span> <span class="o">=</span> <span class="n">SwiftClient</span><span class="p">(</span><span class="n">swiftData</span><span class="p">[</span><span class="mi">0</span><span class="p">],</span> <span class="n">swiftData</span><span class="p">[</span><span class="mi">1</span><span class="p">],</span> <span class="n">swiftData</span><span class="p">[</span><span class="mi">2</span><span class="p">])</span></div><div class='line' id='LC125'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backupToCloudObj</span> <span class="o">=</span> <span class="n">BackupToCloud</span><span class="p">(</span><span class="n">fileList</span><span class="p">,</span> <span class="n">swiftObj</span><span class="p">)</span></div><div class='line' id='LC126'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backuptime</span> <span class="o">=</span> <span class="n">backupToCloudObj</span><span class="o">.</span><span class="n">backup</span><span class="p">()</span></div><div class='line' id='LC127'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;result&#39;</span>  <span class="p">:</span> <span class="bp">True</span><span class="p">,</span></div><div class='line' id='LC128'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;code&#39;</span>    <span class="p">:</span> <span class="s">&#39;100&#39;</span><span class="p">,</span></div><div class='line' id='LC129'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;data&#39;</span>    <span class="p">:</span> <span class="p">{</span><span class="s">&#39;backup_time&#39;</span><span class="p">:</span> <span class="n">backuptime</span><span class="p">},</span></div><div class='line' id='LC130'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;msg&#39;</span>     <span class="p">:</span> <span class="s">&#39;config backup success&#39;</span><span class="p">}</span></div><div class='line' id='LC131'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">json</span><span class="o">.</span><span class="n">dumps</span><span class="p">(</span><span class="n">return_val</span><span class="p">)</span></div><div class='line' id='LC132'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">except</span> <span class="n">BackupError</span> <span class="k">as</span> <span class="n">e</span><span class="p">:</span></div><div class='line' id='LC133'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;result&#39;</span>  <span class="p">:</span> <span class="bp">False</span><span class="p">,</span></div><div class='line' id='LC134'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;code&#39;</span>    <span class="p">:</span> <span class="n">e</span><span class="o">.</span><span class="n">code</span><span class="p">,</span></div><div class='line' id='LC135'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;data&#39;</span>    <span class="p">:</span> <span class="bp">None</span><span class="p">,</span></div><div class='line' id='LC136'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;msg&#39;</span>     <span class="p">:</span> <span class="n">e</span><span class="o">.</span><span class="n">msg</span><span class="p">}</span></div><div class='line' id='LC137'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">json</span><span class="o">.</span><span class="n">dumps</span><span class="p">(</span><span class="n">return_val</span><span class="p">)</span></div><div class='line' id='LC138'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">except</span> <span class="ne">Exception</span> <span class="k">as</span> <span class="n">e</span><span class="p">:</span></div><div class='line' id='LC139'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">error</span><span class="p">(</span><span class="s">&#39;unexception error: </span><span class="si">%s</span><span class="s">&#39;</span> <span class="o">%</span> <span class="n">e</span><span class="p">)</span></div><div class='line' id='LC140'><br/></div><div class='line' id='LC141'><br/></div><div class='line' id='LC142'><span class="c">#----------------------------------------------------------------------</span></div><div class='line' id='LC143'><span class="k">def</span> <span class="nf">restore_gateway_configuration</span><span class="p">():</span></div><div class='line' id='LC144'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="sd">&quot;&quot;&quot;</span></div><div class='line' id='LC145'><span class="sd">    Restore latest configuration from Cloud.</span></div><div class='line' id='LC146'><span class="sd">    &quot;&quot;&quot;</span></div><div class='line' id='LC147'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">info</span><span class="p">(</span><span class="s">&quot;[2] restore_gateway_configuration start&quot;</span><span class="p">)</span></div><div class='line' id='LC148'><br/></div><div class='line' id='LC149'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">tmp_dir</span> <span class="o">=</span> <span class="s">&quot;/tmp/restore_config/&quot;</span></div><div class='line' id='LC150'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="s">&quot;rm -r &quot;</span> <span class="o">+</span> <span class="n">tmp_dir</span><span class="p">)</span>          <span class="c"># clean old temp. data</span></div><div class='line' id='LC151'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="s">&quot;mkdir -p &quot;</span> <span class="o">+</span> <span class="n">tmp_dir</span><span class="p">)</span>       <span class="c"># prepare tmp working directory.</span></div><div class='line' id='LC152'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">backup_info</span> <span class="o">=</span> <span class="n">_get_latest_backup</span><span class="p">()</span>     <span class="c"># read backup file info from cloud.</span></div><div class='line' id='LC153'><br/></div><div class='line' id='LC154'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">if</span> <span class="n">backup_info</span> <span class="ow">is</span> <span class="bp">None</span><span class="p">:</span></div><div class='line' id='LC155'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_ok</span> <span class="o">=</span> <span class="bp">False</span></div><div class='line' id='LC156'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_code</span> <span class="o">=</span> <span class="s">&quot;000&quot;</span></div><div class='line' id='LC157'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_msg</span> <span class="o">=</span> <span class="s">&quot;There is no [config] container at Swift.&quot;</span></div><div class='line' id='LC158'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">else</span><span class="p">:</span></div><div class='line' id='LC159'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">fname</span> <span class="o">=</span> <span class="n">backup_info</span><span class="p">[</span><span class="s">&#39;fname&#39;</span><span class="p">]</span></div><div class='line' id='LC160'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="p">[</span><span class="n">url</span><span class="p">,</span> <span class="n">login</span><span class="p">,</span> <span class="n">password</span><span class="p">]</span> <span class="o">=</span> <span class="n">_get_Swift_credential</span><span class="p">()</span></div><div class='line' id='LC161'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;cd </span><span class="si">%s</span><span class="s">; &quot;</span> <span class="o">%</span> <span class="p">(</span><span class="n">tmp_dir</span><span class="p">)</span></div><div class='line' id='LC162'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">+=</span> <span class="s">&quot;swift -A https://</span><span class="si">%s</span><span class="s">/auth/v1.0 -U </span><span class="si">%s</span><span class="s"> -K </span><span class="si">%s</span><span class="s"> download config </span><span class="si">%s</span><span class="s">&quot;</span>\</div><div class='line' id='LC163'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="o">%</span> <span class="p">(</span><span class="n">url</span><span class="p">,</span> <span class="n">login</span><span class="p">,</span> <span class="n">password</span><span class="p">,</span> <span class="n">fname</span><span class="p">)</span></div><div class='line' id='LC164'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="n">cmd</span><span class="p">)</span></div><div class='line' id='LC165'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 1. download last backup file.</span></div><div class='line' id='LC166'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;cd </span><span class="si">%s</span><span class="s">; tar zxvf </span><span class="si">%s</span><span class="s"> &quot;</span> <span class="o">%</span> <span class="p">(</span><span class="n">tmp_dir</span><span class="p">,</span> <span class="n">fname</span><span class="p">)</span></div><div class='line' id='LC167'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="n">cmd</span><span class="p">)</span></div><div class='line' id='LC168'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 2. untar the backup file.</span></div><div class='line' id='LC169'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">try</span><span class="p">:</span></div><div class='line' id='LC170'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">fp</span> <span class="o">=</span> <span class="nb">open</span><span class="p">(</span><span class="n">tmp_dir</span> <span class="o">+</span> <span class="s">&#39;metadata.txt&#39;</span><span class="p">)</span></div><div class='line' id='LC171'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">JsonData</span> <span class="o">=</span> <span class="n">fp</span><span class="o">.</span><span class="n">read</span><span class="p">()</span></div><div class='line' id='LC172'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">bak</span> <span class="o">=</span> <span class="n">json</span><span class="o">.</span><span class="n">loads</span><span class="p">(</span><span class="n">JsonData</span><span class="p">)</span></div><div class='line' id='LC173'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">for</span> <span class="n">b</span> <span class="ow">in</span> <span class="n">bak</span><span class="o">.</span><span class="n">items</span><span class="p">():</span></div><div class='line' id='LC174'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">v</span> <span class="o">=</span> <span class="n">b</span><span class="p">[</span><span class="mi">1</span><span class="p">]</span>    <span class="c"># get a dict of &#39;file&#39;</span></div><div class='line' id='LC175'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ToDo...</span></div><div class='line' id='LC176'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 3.1. upgrade config files if necessary.</span></div><div class='line' id='LC177'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;chown </span><span class="si">%s</span><span class="s">:</span><span class="si">%s</span><span class="s"> </span><span class="si">%s%s</span><span class="s">&quot;</span> <span class="o">%</span> <span class="p">(</span><span class="n">v</span><span class="p">[</span><span class="s">&#39;user&#39;</span><span class="p">],</span> <span class="n">v</span><span class="p">[</span><span class="s">&#39;group&#39;</span><span class="p">],</span> \</div><div class='line' id='LC178'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">tmp_dir</span><span class="p">,</span> <span class="n">v</span><span class="p">[</span><span class="s">&#39;fname&#39;</span><span class="p">])</span></div><div class='line' id='LC179'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="n">cmd</span><span class="p">)</span>        <span class="c"># chage file owner</span></div><div class='line' id='LC180'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;chmod </span><span class="si">%s</span><span class="s"> </span><span class="si">%s%s</span><span class="s">&quot;</span> <span class="o">%</span> <span class="p">(</span><span class="n">v</span><span class="p">[</span><span class="s">&#39;chmod&#39;</span><span class="p">],</span> <span class="n">tmp_dir</span><span class="p">,</span> <span class="n">v</span><span class="p">[</span><span class="s">&#39;fname&#39;</span><span class="p">])</span></div><div class='line' id='LC181'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="n">cmd</span><span class="p">)</span>        <span class="c"># chage file access</span></div><div class='line' id='LC182'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">cmd</span> <span class="o">=</span> <span class="s">&quot;cd </span><span class="si">%s</span><span class="s">; mv </span><span class="si">%s</span><span class="s"> </span><span class="si">%s</span><span class="s">&quot;</span> <span class="o">%</span> <span class="p">(</span><span class="n">tmp_dir</span><span class="p">,</span> <span class="n">v</span><span class="p">[</span><span class="s">&#39;fname&#39;</span><span class="p">],</span> <span class="n">v</span><span class="p">[</span><span class="s">&#39;fpath&#39;</span><span class="p">])</span></div><div class='line' id='LC183'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">os</span><span class="o">.</span><span class="n">system</span><span class="p">(</span><span class="n">cmd</span><span class="p">)</span></div><div class='line' id='LC184'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 3.2. put config files back to their destination folder.</span></div><div class='line' id='LC185'><br/></div><div class='line' id='LC186'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 3.3. restart gateway services</span></div><div class='line' id='LC187'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">api</span><span class="o">.</span><span class="n">restart_nfs_service</span><span class="p">()</span></div><div class='line' id='LC188'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">api</span><span class="o">.</span><span class="n">restart_smb_service</span><span class="p">()</span></div><div class='line' id='LC189'><br/></div><div class='line' id='LC190'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_ok</span> <span class="o">=</span> <span class="bp">True</span></div><div class='line' id='LC191'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_code</span> <span class="o">=</span> <span class="s">&quot;100&quot;</span></div><div class='line' id='LC192'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_msg</span> <span class="o">=</span> <span class="bp">None</span></div><div class='line' id='LC193'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="c"># ^^^ 3. parse metadata. (where should config files be put to)</span></div><div class='line' id='LC194'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">except</span><span class="p">:</span></div><div class='line' id='LC195'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_ok</span> <span class="o">=</span> <span class="bp">False</span></div><div class='line' id='LC196'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_code</span> <span class="o">=</span> <span class="s">&quot;001&quot;</span></div><div class='line' id='LC197'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">op_msg</span> <span class="o">=</span> <span class="s">&quot;Errors occurred when restoring configuration files.&quot;</span></div><div class='line' id='LC198'><br/></div><div class='line' id='LC199'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ end of if-else</span></div><div class='line' id='LC200'><br/></div><div class='line' id='LC201'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">return_val</span> <span class="o">=</span> <span class="p">{</span><span class="s">&#39;result&#39;</span><span class="p">:</span> <span class="n">op_ok</span><span class="p">,</span></div><div class='line' id='LC202'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;code&#39;</span><span class="p">:</span>   <span class="n">op_code</span><span class="p">,</span></div><div class='line' id='LC203'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span class="s">&#39;msg&#39;</span><span class="p">:</span>    <span class="n">op_msg</span><span class="p">}</span></div><div class='line' id='LC204'><br/></div><div class='line' id='LC205'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">log</span><span class="o">.</span><span class="n">info</span><span class="p">(</span><span class="s">&quot;[2] restore_gateway_configuration stop&quot;</span><span class="p">)</span></div><div class='line' id='LC206'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">return</span> <span class="n">json</span><span class="o">.</span><span class="n">dumps</span><span class="p">(</span><span class="n">return_val</span><span class="p">)</span></div><div class='line' id='LC207'><br/></div><div class='line' id='LC208'><br/></div><div class='line' id='LC209'><span class="c">#----------------------------------------------------------------------</span></div><div class='line' id='LC210'><span class="k">if</span> <span class="n">__name__</span> <span class="o">==</span> <span class="s">&#39;__main__&#39;</span><span class="p">:</span></div><div class='line' id='LC211'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ res = save_gateway_configuration()</span></div><div class='line' id='LC212'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ print res</span></div><div class='line' id='LC213'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="n">info</span> <span class="o">=</span> <span class="n">get_configuration_backup_info</span><span class="p">()</span></div><div class='line' id='LC214'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">print</span> <span class="n">info</span></div><div class='line' id='LC215'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ res = restore_gateway_configuration()</span></div><div class='line' id='LC216'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ #res = save_gateway_configuration()</span></div><div class='line' id='LC217'><br/></div><div class='line' id='LC218'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="c">#~ print res</span></div><div class='line' id='LC219'>&nbsp;&nbsp;&nbsp;&nbsp;<span class="k">pass</span></div></pre></div>
          </td>
        </tr>
      </table>
  </div>

          </div>
        </div>
      </div>
    </div>

  </div>

<div class="frame frame-loading large-loading-area" style="display:none;" data-tree-list-url="/Delta-Cloud/StorageAppliance/tree-list/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267" data-blob-url-prefix="/Delta-Cloud/StorageAppliance/blob/3dbee139cdd8fb77c3d3793e3afaf08cd09d8267">
  <img src="https://a248.e.akamai.net/assets.github.com/images/spinners/octocat-spinner-64.gif?1334862346" height="64" width="64">
</div>

        </div>
      </div>
      <div class="context-overlay"></div>
    </div>

      <div id="footer-push"></div><!-- hack for sticky footer -->
    </div><!-- end of wrapper - hack for sticky footer -->

      <!-- footer -->
      <div id="footer" >
        
  <div class="upper_footer">
     <div class="container clearfix">

       <!--[if IE]><h4 id="blacktocat_ie">GitHub Links</h4><![endif]-->
       <![if !IE]><h4 id="blacktocat">GitHub Links</h4><![endif]>

       <ul class="footer_nav">
         <h4>GitHub</h4>
         <li><a href="https://github.com/about">About</a></li>
         <li><a href="https://github.com/blog">Blog</a></li>
         <li><a href="https://github.com/features">Features</a></li>
         <li><a href="https://github.com/contact">Contact &amp; Support</a></li>
         <li><a href="https://github.com/training">Training</a></li>
         <li><a href="http://enterprise.github.com/">GitHub Enterprise</a></li>
         <li><a href="http://status.github.com/">Site Status</a></li>
       </ul>

       <ul class="footer_nav">
         <h4>Clients</h4>
         <li><a href="http://mac.github.com/">GitHub for Mac</a></li>
         <li><a href="http://windows.github.com/">GitHub for Windows</a></li>
         <li><a href="http://eclipse.github.com/">GitHub for Eclipse</a></li>
         <li><a href="http://mobile.github.com/">GitHub Mobile Apps</a></li>
       </ul>

       <ul class="footer_nav">
         <h4>Tools</h4>
         <li><a href="http://get.gaug.es/">Gauges: Web analytics</a></li>
         <li><a href="http://speakerdeck.com">Speaker Deck: Presentations</a></li>
         <li><a href="https://gist.github.com">Gist: Code snippets</a></li>

         <h4 class="second">Extras</h4>
         <li><a href="http://jobs.github.com/">Job Board</a></li>
         <li><a href="http://shop.github.com/">GitHub Shop</a></li>
         <li><a href="http://octodex.github.com/">The Octodex</a></li>
       </ul>

       <ul class="footer_nav">
         <h4>Documentation</h4>
         <li><a href="http://help.github.com/">GitHub Help</a></li>
         <li><a href="http://developer.github.com/">Developer API</a></li>
         <li><a href="http://github.github.com/github-flavored-markdown/">GitHub Flavored Markdown</a></li>
         <li><a href="http://pages.github.com/">GitHub Pages</a></li>
       </ul>

     </div><!-- /.site -->
  </div><!-- /.upper_footer -->

<div class="lower_footer">
  <div class="container clearfix">
    <!--[if IE]><div id="legal_ie"><![endif]-->
    <![if !IE]><div id="legal"><![endif]>
      <ul>
          <li><a href="https://github.com/site/terms">Terms of Service</a></li>
          <li><a href="https://github.com/site/privacy">Privacy</a></li>
          <li><a href="https://github.com/security">Security</a></li>
      </ul>

      <p>&copy; 2012 <span title="0.25619s from fe13.rs.github.com">GitHub</span> Inc. All rights reserved.</p>
    </div><!-- /#legal or /#legal_ie-->

      <div class="sponsor">
        <a href="http://www.rackspace.com" class="logo">
          <img alt="Dedicated Server" height="36" src="https://a248.e.akamai.net/assets.github.com/images/modules/footer/rackspaces_logo.png?1334862346" width="38" />
        </a>
        Powered by the <a href="http://www.rackspace.com ">Dedicated
        Servers</a> and<br/> <a href="http://www.rackspacecloud.com">Cloud
        Computing</a> of Rackspace Hosting<span>&reg;</span>
      </div>
  </div><!-- /.site -->
</div><!-- /.lower_footer -->

      </div><!-- /#footer -->

    

<div id="keyboard_shortcuts_pane" class="instapaper_ignore readability-extra" style="display:none">
  <h2>Keyboard Shortcuts <small><a href="#" class="js-see-all-keyboard-shortcuts">(see all)</a></small></h2>

  <div class="columns threecols">
    <div class="column first">
      <h3>Site wide shortcuts</h3>
      <dl class="keyboard-mappings">
        <dt>s</dt>
        <dd>Focus site search</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>?</dt>
        <dd>Bring up this help dialog</dd>
      </dl>
    </div><!-- /.column.first -->

    <div class="column middle" style='display:none'>
      <h3>Commit list</h3>
      <dl class="keyboard-mappings">
        <dt>j</dt>
        <dd>Move selection down</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>k</dt>
        <dd>Move selection up</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>c <em>or</em> o <em>or</em> enter</dt>
        <dd>Open commit</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>y</dt>
        <dd>Expand URL to its canonical form</dd>
      </dl>
    </div><!-- /.column.first -->

    <div class="column last" style='display:none'>
      <h3>Pull request list</h3>
      <dl class="keyboard-mappings">
        <dt>j</dt>
        <dd>Move selection down</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>k</dt>
        <dd>Move selection up</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt>o <em>or</em> enter</dt>
        <dd>Open issue</dd>
      </dl>
      <dl class="keyboard-mappings">
        <dt><span class="platform-mac">⌘</span><span class="platform-other">ctrl</span> <em>+</em> enter</dt>
        <dd>Submit comment</dd>
      </dl>
    </div><!-- /.columns.last -->

  </div><!-- /.columns.equacols -->

  <div style='display:none'>
    <div class="rule"></div>

    <h3>Issues</h3>

    <div class="columns threecols">
      <div class="column first">
        <dl class="keyboard-mappings">
          <dt>j</dt>
          <dd>Move selection down</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>k</dt>
          <dd>Move selection up</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>x</dt>
          <dd>Toggle selection</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>o <em>or</em> enter</dt>
          <dd>Open issue</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt><span class="platform-mac">⌘</span><span class="platform-other">ctrl</span> <em>+</em> enter</dt>
          <dd>Submit comment</dd>
        </dl>
      </div><!-- /.column.first -->
      <div class="column last">
        <dl class="keyboard-mappings">
          <dt>c</dt>
          <dd>Create issue</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>l</dt>
          <dd>Create label</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>i</dt>
          <dd>Back to inbox</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>u</dt>
          <dd>Back to issues</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>/</dt>
          <dd>Focus issues search</dd>
        </dl>
      </div>
    </div>
  </div>

  <div style='display:none'>
    <div class="rule"></div>

    <h3>Issues Dashboard</h3>

    <div class="columns threecols">
      <div class="column first">
        <dl class="keyboard-mappings">
          <dt>j</dt>
          <dd>Move selection down</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>k</dt>
          <dd>Move selection up</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>o <em>or</em> enter</dt>
          <dd>Open issue</dd>
        </dl>
      </div><!-- /.column.first -->
    </div>
  </div>

  <div style='display:none'>
    <div class="rule"></div>

    <h3>Network Graph</h3>
    <div class="columns equacols">
      <div class="column first">
        <dl class="keyboard-mappings">
          <dt><span class="badmono">←</span> <em>or</em> h</dt>
          <dd>Scroll left</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt><span class="badmono">→</span> <em>or</em> l</dt>
          <dd>Scroll right</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt><span class="badmono">↑</span> <em>or</em> k</dt>
          <dd>Scroll up</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt><span class="badmono">↓</span> <em>or</em> j</dt>
          <dd>Scroll down</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>t</dt>
          <dd>Toggle visibility of head labels</dd>
        </dl>
      </div><!-- /.column.first -->
      <div class="column last">
        <dl class="keyboard-mappings">
          <dt>shift <span class="badmono">←</span> <em>or</em> shift h</dt>
          <dd>Scroll all the way left</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>shift <span class="badmono">→</span> <em>or</em> shift l</dt>
          <dd>Scroll all the way right</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>shift <span class="badmono">↑</span> <em>or</em> shift k</dt>
          <dd>Scroll all the way up</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>shift <span class="badmono">↓</span> <em>or</em> shift j</dt>
          <dd>Scroll all the way down</dd>
        </dl>
      </div><!-- /.column.last -->
    </div>
  </div>

  <div >
    <div class="rule"></div>
    <div class="columns threecols">
      <div class="column first" >
        <h3>Source Code Browsing</h3>
        <dl class="keyboard-mappings">
          <dt>t</dt>
          <dd>Activates the file finder</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>l</dt>
          <dd>Jump to line</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>w</dt>
          <dd>Switch branch/tag</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>y</dt>
          <dd>Expand URL to its canonical form</dd>
        </dl>
      </div>
    </div>
  </div>

  <div style='display:none'>
    <div class="rule"></div>
    <div class="columns threecols">
      <div class="column first">
        <h3>Browsing Commits</h3>
        <dl class="keyboard-mappings">
          <dt><span class="platform-mac">⌘</span><span class="platform-other">ctrl</span> <em>+</em> enter</dt>
          <dd>Submit comment</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>escape</dt>
          <dd>Close form</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>p</dt>
          <dd>Parent commit</dd>
        </dl>
        <dl class="keyboard-mappings">
          <dt>o</dt>
          <dd>Other parent commit</dd>
        </dl>
      </div>
    </div>
  </div>
</div>

    <div id="markdown-help" class="instapaper_ignore readability-extra">
  <h2>Markdown Cheat Sheet</h2>

  <div class="cheatsheet-content">

  <div class="mod">
    <div class="col">
      <h3>Format Text</h3>
      <p>Headers</p>
      <pre>
# This is an &lt;h1&gt; tag
## This is an &lt;h2&gt; tag
###### This is an &lt;h6&gt; tag</pre>
     <p>Text styles</p>
     <pre>
*This text will be italic*
_This will also be italic_
**This text will be bold**
__This will also be bold__

*You **can** combine them*
</pre>
    </div>
    <div class="col">
      <h3>Lists</h3>
      <p>Unordered</p>
      <pre>
* Item 1
* Item 2
  * Item 2a
  * Item 2b</pre>
     <p>Ordered</p>
     <pre>
1. Item 1
2. Item 2
3. Item 3
   * Item 3a
   * Item 3b</pre>
    </div>
    <div class="col">
      <h3>Miscellaneous</h3>
      <p>Images</p>
      <pre>
![GitHub Logo](/images/logo.png)
Format: ![Alt Text](url)
</pre>
     <p>Links</p>
     <pre>
http://github.com - automatic!
[GitHub](http://github.com)</pre>
<p>Blockquotes</p>
     <pre>
As Kanye West said:

> We're living the future so
> the present is our past.
</pre>
    </div>
  </div>
  <div class="rule"></div>

  <h3>Code Examples in Markdown</h3>
  <div class="col">
      <p>Syntax highlighting with <a href="http://github.github.com/github-flavored-markdown/" title="GitHub Flavored Markdown" target="_blank">GFM</a></p>
      <pre>
```javascript
function fancyAlert(arg) {
  if(arg) {
    $.facebox({div:'#foo'})
  }
}
```</pre>
    </div>
    <div class="col">
      <p>Or, indent your code 4 spaces</p>
      <pre>
Here is a Python code example
without syntax highlighting:

    def foo:
      if not bar:
        return true</pre>
    </div>
    <div class="col">
      <p>Inline code for comments</p>
      <pre>
I think you should use an
`&lt;addr&gt;` element here instead.</pre>
    </div>
  </div>

  </div>
</div>


    <div id="ajax-error-message">
      <span class="mini-icon mini-icon-exclamation"></span>
      Something went wrong with that request. Please try again.
      <a href="#" class="ajax-error-dismiss">Dismiss</a>
    </div>

    <div id="logo-popup">
      <h2>Looking for the GitHub logo?</h2>
      <ul>
        <li>
          <h4>GitHub Logo</h4>
          <a href="http://github-media-downloads.s3.amazonaws.com/GitHub_Logos.zip"><img alt="Github_logo" src="https://a248.e.akamai.net/assets.github.com/images/modules/about_page/github_logo.png?1334862345" /></a>
          <a href="http://github-media-downloads.s3.amazonaws.com/GitHub_Logos.zip" class="minibutton btn-download download">Download</a>
        </li>
        <li>
          <h4>The Octocat</h4>
          <a href="http://github-media-downloads.s3.amazonaws.com/Octocats.zip"><img alt="Octocat" src="https://a248.e.akamai.net/assets.github.com/images/modules/about_page/octocat.png?1334862345" /></a>
          <a href="http://github-media-downloads.s3.amazonaws.com/Octocats.zip" class="minibutton btn-download download">Download</a>
        </li>
      </ul>
    </div>

    
    <span id='server_response_time' data-time='0.25888' data-host='fe13'></span>
  </body>
</html>

