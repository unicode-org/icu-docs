# How to Update `icu-docs`

1. Create a personal fork of <https://github.com/unicode-org/icu-docs>
1. Clone the repository from your personal fork to create a local copy on your system
1. In your local working copy, create a feature branch (`<FEATURE-BRANCH>`)
1. Following the beginning of the [typical git workflow](https://icu.unicode.org/repository/gitdev):
    a. Make any changes you want, then make a commit
    b. Push your feature branch to your personal fork on Github
1. On Github, in your own personal fork of `icu-docs`, turn on GitHub Pages
hosting with the **Source:** set to your new **`<FEATURE-BRANCH>` branch**.
For example, go to the settings page at https://github.com/srl295/icu-docs/settings (use your own username)
1. You will see a note:
> ✓ Your site is published at <https://srl295.github.io/icu-docs/>
1. You can use the above URL to view the changes you have made on your feature branch in your fork.
    a. You may need to wait ~ 5 minutes ([up to 20 mins](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/about-jekyll-build-errors-for-github-pages-sites)) to see the changes that you pushed to your fork be re-rendered and visible on your site.
1. After verifying that the rendered changes on your personal site look good, open a PR from your feature branch on your fork with a destination of the upstream (`unicode-org/icu-docs`) repository's `main` branch.

### License

Please see [./LICENSE](./LICENSE)

> Copyright © 2016 and later Unicode, Inc. and others. All Rights Reserved.
Unicode and the Unicode Logo are registered trademarks
of Unicode, Inc. in the U.S. and other countries.
[Terms of Use and License](http://www.unicode.org/copyright.html)
