# Changelog

## [0.1.0](https://github.com/ScottKirvan/UE_Cambriana/compare/v0.0.0...v0.1.0) (2026-11-26)


### Features

* .lsys sides, surface and minthickness win over asset and actor values unless the actor's override is on ([80c997e](https://github.com/ScottKirvan/UE_Cambriana/commit/80c997e7ea62c5efac5e38ea550270dc79b6b3cc))
* export grammar assets as .lsys files, converting legacy text ([6b78d7b](https://github.com/ScottKirvan/UE_Cambriana/commit/6b78d7b4da51668616b250f794994414364111a0))
* grammar assets hold editable size and surfacer settings ([abe3440](https://github.com/ScottKirvan/UE_Cambriana/commit/abe3440576e9d5bcbb623c0f0823de2983c6b2ad))
* grammar assets open in a grammar editor with Rules and Details tabs, and Rules edits update the asset with undo ([e14aa79](https://github.com/ScottKirvan/UE_Cambriana/commit/e14aa799bf825e8c263b2149b5357f590647cfe7))
* grammar editor Details shows the grammar's parameters grouped by tier, stores edited values on the asset for the preview and referencing live actors, and keeps the seed field and new-seed button ([8641e7d](https://github.com/ScottKirvan/UE_Cambriana/commit/8641e7da9d151c79176b426d2fa83b20ad97eb9c))
* grammar editor edits a transient working copy; Save copies it into the asset and rebuilds placed actors once; closing with unsaved edits asks to save, discard or cancel ([8b4edfa](https://github.com/ScottKirvan/UE_Cambriana/commit/8b4edfa4b3d2a6c66baae3f3b128aaf20a5dfec1))
* grammar editor shows a transient preview of the asset in a Viewport tab, beside Rules, Details and an empty Outputs tab, rebuilt when the asset changes ([2455a74](https://github.com/ScottKirvan/UE_Cambriana/commit/2455a74de98f431c866b83e0a88bf066e8e9de30))
* grammar editor shows the build's errors and warnings in the Rules tab at their line and column, or production position, and clears them when fixed ([10083c5](https://github.com/ScottKirvan/UE_Cambriana/commit/10083c5f53a600a07a1bf4fc331ce003fcf2540f))
* import .ls and .lsys files as grammar assets ([24f9697](https://github.com/ScottKirvan/UE_Cambriana/commit/24f96976cde5a438ee6b96c39bf60185fc6eae8c))
* live actor builds .lsys text through the core ([f4677b4](https://github.com/ScottKirvan/UE_Cambriana/commit/f4677b4af3be28f1be80ff1be018823311efced4))
* live actor overrides for sides, closed form and min thickness win over a grammar asset's settings ([15900da](https://github.com/ScottKirvan/UE_Cambriana/commit/15900dacc39fc00cac28e2836579e29fc2a44174))
* live actor size override for .lsys grammars ([d4dac5e](https://github.com/ScottKirvan/UE_Cambriana/commit/d4dac5e949a40345c47b88a3236014cd1efead67))
* live actors referencing a grammar asset build with its size and surfacer settings ([0112962](https://github.com/ScottKirvan/UE_Cambriana/commit/01129628c38f8d43995dc26fb8731478306b5382))
* new grammar assets hold the default grammar as .lsys text ([00c23fd](https://github.com/ScottKirvan/UE_Cambriana/commit/00c23fd27310baf0f8161c9514b2b69e3fb3815e))
* reimport grammar assets from their source files ([269dbcd](https://github.com/ScottKirvan/UE_Cambriana/commit/269dbcd76f91ee08243e9854a155ff3238484bc1))


### Bug Fixes

* a grammar being loaded takes its import data from its package, as UTexture does, and one loaded without any gets some ([d615457](https://github.com/ScottKirvan/UE_Cambriana/commit/d615457c0403ff90415c68bd25e9fec329e209af))
* a live actor setting's reset arrow resets it to the value it would start from at placement, not the class default ([ae6d4ad](https://github.com/ScottKirvan/UE_Cambriana/commit/ae6d4aded51bfb573e07b10257fbd12053adde3c))
* add Dependabot config for monthly grouped dependency updates ([42493d2](https://github.com/ScottKirvan/UE_Cambriana/commit/42493d242d7c79225347f94578cd6d9d09eea400))
* an undo during the Rules typing pause undoes the typed edit as one step, and one from outside the editor keeps the typed text ([450c2c5](https://github.com/ScottKirvan/UE_Cambriana/commit/450c2c562e882fa4f261de56fda639f7c6bbac7c))
* Boju Studios producer and category, actor shows the settings its build used, reimport menu ([8c68a81](https://github.com/ScottKirvan/UE_Cambriana/commit/8c68a81fbb2ce99c6323bde9a9da76d9d07cdf8e))
* build inputs compare grammar text case-sensitively, so a case-only edit rebuilds the live actor ([78a0c19](https://github.com/ScottKirvan/UE_Cambriana/commit/78a0c191c698de9a9f13c8f245f176870b134d1b))
* each live actor override checkbox is inline, left of the values it controls ([5c31b3f](https://github.com/ScottKirvan/UE_Cambriana/commit/5c31b3fe70ed51e99abdcf73f14f49c826a48a02)), closes [#18](https://github.com/ScottKirvan/UE_Cambriana/issues/18)
* grammar Details refresh after a parameter is added, removed or renamed no longer holds the old layout alive ([be58281](https://github.com/ScottKirvan/UE_Cambriana/commit/be5828173c4ce5f77654e27eee78052b1b6315c6))
* grammar editor closes that cannot be stopped ask save or discard, ask again after a failed save, and do not ask twice ([1f7a2da](https://github.com/ScottKirvan/UE_Cambriana/commit/1f7a2da359d580756187c14f53349ef267378c08))
* grammar editor preview rests the floor under the mesh after each rebuild, and the camera frames the mesh on open and on F ([e7781dd](https://github.com/ScottKirvan/UE_Cambriana/commit/e7781dd92a98cffe0f74df547287b2803b8c83bb))
* grammar editor Save As onto an existing asset's path leaves that asset alone and tells the user; the dialog disallows existing assets ([a673a4a](https://github.com/ScottKirvan/UE_Cambriana/commit/a673a4a97fad6a8cbee90df9fc5fedcf3dc1fee9))
* grammar editor Save As writes the edits to the new asset only, and a save that does not complete leaves the asset and its actors and keeps the editor open with its edits ([bcf8610](https://github.com/ScottKirvan/UE_Cambriana/commit/bcf86101dde99c2608703fd926cb956814284a95))
* grammar editor viewport redraws when the preview rebuilds, without waiting for input ([487310c](https://github.com/ScottKirvan/UE_Cambriana/commit/487310c35d6c9a46da1692a79db8f4ce5b89ce26))
* live actor size and surfacer settings are editable whenever the build uses the actor's value, with their override checkbox inline ([0ae5823](https://github.com/ScottKirvan/UE_Cambriana/commit/0ae5823d0367f4d45e2c049454b46a76c3170041))
* placed live actors and new or imported grammar assets start their settings from the grammar text ([636c4dd](https://github.com/ScottKirvan/UE_Cambriana/commit/636c4dd7e6fa955698c7fa9d9094484a30a61eb0))
* register the Boju Studios placement category after engine init ([4334c26](https://github.com/ScottKirvan/UE_Cambriana/commit/4334c264fbea98aacbcb2d82d748d713cbdb2c57))
* rejected legacy import test uses its original input and requires every error ([9fca580](https://github.com/ScottKirvan/UE_Cambriana/commit/9fca5800748e68058a8d9b4628b113710661db05))
* Rules edits reach the grammar after a typing pause, before a save and on close, and editor callbacks do not outlive the editor ([8ada837](https://github.com/ScottKirvan/UE_Cambriana/commit/8ada83748c12633725206147fc63d88c10934cf1))
* Rules edits reach the grammar with \n line breaks, so typed text matches on Windows ([b0abe58](https://github.com/ScottKirvan/UE_Cambriana/commit/b0abe580dfde1e5bef80b05ccf571011d7e23e0c))
* the editor module registers the placement category when it starts after engine init, and its shutdown unbinds the engine-init callback ([0d598c7](https://github.com/ScottKirvan/UE_Cambriana/commit/0d598c70c2928a9bfd8f4a0260273b74e4321d15))
* the grammar editor's Details hides the working copy's import data, which a save would drop ([f9a101f](https://github.com/ScottKirvan/UE_Cambriana/commit/f9a101fd9a13015b1b5560bf5636e810e9757d15))
* turning a live actor override off no longer resets the actor's value ([ed15e7b](https://github.com/ScottKirvan/UE_Cambriana/commit/ed15e7b4e0d116ec3a9b9a613a24aabbb04fa361))
* tweak to default materials ([4b15714](https://github.com/ScottKirvan/UE_Cambriana/commit/4b15714327d5108134e2686703e998444a7e55bc))

## 0.0.0 (2026-11-05)


### Features

* interim grammar asset and live actor with generation seam ([90be7a1](https://github.com/ScottKirvan/UE_Cambriana/commit/90be7a1089cd54d0a3bb9fae901749a054a479a1))
* live actor builds real geometry through the core ([0e79709](https://github.com/ScottKirvan/UE_Cambriana/commit/0e79709120ddcf2be89ff89e6048854b0a321211))
* live actor placed from a grammar asset copies the asset's seed ([3fa3656](https://github.com/ScottKirvan/UE_Cambriana/commit/3fa36564761e53387700af6eac1e7d30825d2afa))


### Bug Fixes

* skip live actor rebuild when construction leaves its build input unchanged ([b79d1cb](https://github.com/ScottKirvan/UE_Cambriana/commit/b79d1cbe654d6dd2e5ba1c6d990328aeebb171ce))
* skip rebuilding live actors when a grammar asset's seed changes ([a8ea98d](https://github.com/ScottKirvan/UE_Cambriana/commit/a8ea98d9e57a665d0e1467550cdca255e6b293e0))

## Changelog
>[!NOTE]
> This file and it's version format is automatically 
> generated by [Please-Release](https://github.com/googleapis/release-please-action), 
> and adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
