// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that links are updated properly when inserting a new property.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #pseudo::after {
          pseudo-property: "12";
          color: red;
      }

      #pseudo::after {
          border: 1px solid black;
      }

      #pseudo::before {
          color: blue;
      }
      </style>
      <div id="container" class="left-intact should-change">
      Red text here.
      </div>

      <div id="other"></div>

      <section id="pseudo">



      </section>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/styles-update-links-2.css');
  await TestRunner.addStylesheetTag('../styles/resources/styles-update-links.css');

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('container', next);
    },

    async function testInsertProperty(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      var treeElement = treeItem.section().addNewBlankProperty(1);
      ElementsTestRunner.waitForStyleApplied(onPropertyInserted);
      treeElement.applyStyleText('PROPERTY: INSERTED;', true);

      async function onPropertyInserted() {
        TestRunner.addResult('\n\n#### AFTER PROPERTY INSERTION ####\n\n');
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        var rules = ElementsTestRunner.getMatchedRules();
        ElementsTestRunner.validateRuleRanges('container', rules, next);
      }
    }
  ]);
})();
