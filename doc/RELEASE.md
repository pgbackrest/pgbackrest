# Release Build Instructions

## Generate Coverage Report

These instructions are temporary until a fully automated report is implemented.

- In `test/src/lcov.conf` remove:
```
 # Specify the regular expression of lines to exclude
 lcov_excl_line=\{\+*uncovered|\{\+*uncoverable

 # Coverage rate limits
 genhtml_hi_limit = 100
 genhtml_med_limit = 90
```

- In `test/lib/pgBackRestTest/Common/JobTest.pm` modify:
```
if (!$bTest || $iTotalLines != $iCoveredLines || $iTotalBranches != $iCoveredBranches)
```
to:
```
if (!$bTest)
```

- Run:
```
/backrest/test/test.pl --dev-test --vm=u18 --c-only
```

- Copy coverage report:
```
cd <pgbackrest-base>/doc/site
rm -rf coverage
cp -r ../../test/coverage/c coverage
```

- In `doc/site/coverage` replace:
```
  <title>LCOV - all.lcov</title>
```
with:
```
  <title>pgBackRest vX.XX C Code Coverage</title>
```

- In `doc/site/coverage` replace:
```
    <tr><td class="title">LCOV - code coverage report</td></tr>
```
with:
```
    <tr><td class="title">pgBackRest vX.XX C Code Coverage</td></tr>
```

- In `doc/site/coverage` replace:
```
  <title>LCOV - all.lcov -
```
with:
```
  <title>pgBackRest vX.XX C Code Coverage -
```

- In `doc/site/coverage` replace:
```
            <td class="headerValue">all.lcov</td>
```
with:
```
            <td class="headerValue">all C unit</td>
```

- Switch to prior dir and copy coverage:
```
cd ../prior/X.XX
cp -r ../../coverage .
```
