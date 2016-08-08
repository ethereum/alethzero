## Former home of AlethZero, AlethOne and AlethFive (cpp-ethereum)

This repository was formerly the home of the Aleth C++ GUI applications for
[cpp-ethereum](http://www.ethdocs.org/en/latest/ethereum-clients/cpp-ethereum/).

Here is [release announcement](https://www.reddit.com/r/ethereum/comments/3l1nlg/official_release_candidate_binaries_for_alethzero/?st=irln93av&sh=6c96944d)
from when AlethOne and AlethZero were first released:

- AlethOne: Streamlined desktop client for mining
- AlethZero: Power-user desktop client

These applications were discontinued because in each case the cost of maintenance ended up exceeding
the benefits which they were delivering to end-users:

* AlethFive was discontinued in [February 2016](https://github.com/ethereum/alethzero/commit/d3b478ebcff534dc0d0af6aa97e1efb3fe8cf752)
* [AlethOne](https://www.youtube.com/watch?v=R0NTtTsAU9I) was discontinued in [March 2016](https://github.com/ethereum/webthree-umbrella/releases/tag/v1.2.3)
* [AlethZero](https://www.youtube.com/watch?v=vXGH6q43i_k) was discontinued in [July 2016](https://blog.ethereum.org/2016/07/08/c-dev-update-summer-edition/)

Our focus is now on HTML5-based GUI solutions such as [remix](https://github.com/ethereum/remix), which
can be used within browser-solidity, Mist, Atom, Visual Studio Code, and other IDE integrations, rather than
building standalone C++ GUI applications.

* The final version of AlethFive was part of the
[webthree-umbrella-v1.1.4 release](https://github.com/ethereum/webthree-umbrella/releases/tag/v1.1.4)
* The final version of AlethOne was part of the
[webthree-umbrella-v1.2.2 release](https://github.com/ethereum/webthree-umbrella/releases/tag/v1.2.2)
* The final version of AlethZero was part of the
[webthree-umbrella-v1.2.9 release](https://github.com/ethereum/webthree-umbrella/releases/tag/v1.2.9)

All of these releases were prior to the hard-fork of 20th July 2016 (at block #192000), so none of these
releases can be used with the currently active chain.  AlethFive was earlier yet, prior to Homestead.
