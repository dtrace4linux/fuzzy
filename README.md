# fuzzy
Fuzzy filename matching
<pre>

fuzzy.pl -- fuzzy filename matching
Usage: fuzzy.pl [switches] <search-term>

  This is a simple tool to do fuzzy filename matching. It was designed
  as a learning experience/POC, to do fuzzy matching, similar to what
  many IDEs provide. "Cost based" ordering gets fiddly and tricky to debug.

  The initial approach for this was to try various algorithms for
  each item, and then order the results by "best". This code is designed
  to search for filenames - a different set of tests would be
  appropriate if we were doing spelling corrections. Whilst
  this is useful for filenames, it can be used for code variable
  names, since they have the same layout.

  We need to provide list of candidate names to search from. This
  can be provided in a variety of forms:

  * If no switches are provided, then everything in $PATH is chosen.
  * We can provide "-f <filename>" to give a generated or curated list
    of filenames
  * We can use "-s <name>" to add a single entry, for debug purposes.

  The results are listed in best-matching order, and color highlighting
  to show the match.

  We avoid native regular expressions - in reality, you do not
  use these to specify files, and it overly complicates things.

  Here is an example:

  $ fuzzy.pl xt

  Without any switches, this would match, for instance:

  - /usr/bin/xterm
  - /usr/bin/xedit
  ...

  The preference is exact case matching, at the start of the filename.
  (Directory paths are ignored/stripped). "xterm" matches because
  it starts with "xt". "xedit" matches because of the "x" and "t".

  The search pattern is split on "." - so you could say:

  $ fuzzy.pl a.j

  which might be used to match "argumentValidator.java". You can
  omit the extension, or use a prefix/substring to select from
  similar files but different extensions.

Switches:

  -dir <path)
      Scan (find $dir -print) to generate a list of filenames to match
      against.

  -f <filename>
      Specify a list of filenames to be matched against. E.g.

      $ find . -type f > files.lst
      $ fuzzy.pl -f files.lst abc

  -s <name>
      Add just <name> to the scan list - useful for debugging one-shot
      scenarios.

  -v  Verbose - some extra debug.
</pre>
