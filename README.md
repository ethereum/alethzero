## Ethereum C++ Client.

[![Join the chat at https://gitter.im/ethereum/alethzero](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/ethereum/alethzero?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

[![Join the chat at https://gitter.im/ethereum/cpp-ethereum](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/ethereum/cpp-ethereum?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

By Gav Wood et al*, 2013, 2014, 2015.

          | Linux   | OSX | Windows
----------|---------|-----|--------
develop   | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=Linux%20C%2B%2B%20develop%20branch)](https://build.ethdev.com/builders/Linux%20C%2B%2B%20develop%20branch/builds/-1) | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=OSX%20C%2B%2B%20develop%20branch)](https://build.ethdev.com/builders/OSX%20C%2B%2B%20develop%20branch/builds/-1) | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=Windows%20C%2B%2B%20develop%20branch)](https://build.ethdev.com/builders/Windows%20C%2B%2B%20develop%20branch/builds/-1)
master    | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=Linux%20C%2B%2B%20master%20branch)](https://build.ethdev.com/builders/Linux%20C%2B%2B%20master%20branch/builds/-1) | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=OSX%20C%2B%2B%20master%20branch)](https://build.ethdev.com/builders/OSX%20C%2B%2B%20master%20branch/builds/-1) | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=Windows%20C%2B%2B%20master%20branch)](https://build.ethdev.com/builders/Windows%20C%2B%2B%20master%20branch/builds/-1)
evmjit    | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=Linux%20C%2B%2B%20develop%20evmjit)](https://build.ethdev.com/builders/Linux%20C%2B%2B%20develop%20evmjit/builds/-1) | [![Build+Status](https://build.ethdev.com/buildstatusimage?builder=OSX%20C%2B%2B%20develop%20evmjit)](https://build.ethdev.com/builders/OSX%20C%2B%2B%20develop%20evmjit/builds/-1) | N/A

[![Stories in Ready](https://badge.waffle.io/ethereum/cpp-ethereum.png?label=ready&title=Ready)](http://waffle.io/ethereum/cpp-ethereum)

AlethZero is the Hardcore Ethereum Client.

Ethereum is based on a design in an original whitepaper by Vitalik Buterin. This implementation is based on the formal specification of a refinement of that idea detailed in the 'yellow paper' by Gavin Wood. Contributors, builders and testers include:

- *arkpar* (**Arkadiy Paronyan**) Mix, PV61/BlockQueue
- *debris* (**Marek Kotewicz**) JSONRPC, web3.js
- *CJentzsch* (**Christoph Jentzsch**) tests, lots of tests
- *LefterisJP* (**Lefteris Karapetsas**) Solidity, libethash
- *chriseth* (**Christian Reitwiessner**) Solidity
- *subtly* (**Alex Leverington**) libp2p, rlpx
- *yann300* (**Yann Levreau**) Mix
- *LianaHus* (**Liana Husikyan**) Solidity
- *chfast* (**Paweł Bylica**) EVMJIT
- *cubedro* (**Marian Oancea**) web3.js
- *gluk256* (**Vlad Gluhovsky**) Whisper
- *programmerTim* (**Tim Hughes**) libethash-cl

And let's not forget: Caktux (neth, ongoing CI), Eric Lombrozo (original MinGW32 cross-compilation), Marko Simovic (original CI).

### Building

See the [Wiki](https://github.com/ethereum/cpp-ethereum/wiki) for build instructions, compatibility information and build tips. 

### License

All new contributions are under the [MIT license](http://opensource.org/licenses/MIT).
See [LICENSE](LICENSE). Some old contributions are under the [GPLv3 license](http://www.gnu.org/licenses/gpl-3.0.en.html). See [GPLV3_LICENSE](GPLV3_LICENSE).

### Contributing

All new contributions are added under the MIT License. Please refer to the `LICENSE` file in the root directory.
To state that you accept this fact for all of your contributions please add yourself to the list of external contributors like in the example below.

#### External Contributors
I hereby place all my contributions in this codebase under an MIT
licence, as specified [here](http://opensource.org/licenses/MIT).
- *Name Surname* (**email@domain**)

#### Contribution guideline

Please add yourself in the `@author` doxygen  section of the file your are adding/editing
with the same wording as the one you listed yourself in the external contributors section above,
only replacing the word **contribution** by **file**

All development goes in develop branch - please don't submit pull requests to master.

Please read [CodingStandards.txt](CodingStandards.txt) thoroughly before making alterations to the code base. Please do *NOT* use an editor that automatically reformats whitespace away from astylerc or the formatting guidelines as described in [CodingStandards.txt](CodingStandards.txt).

