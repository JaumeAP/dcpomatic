Feature: Add still image

Scenario Outline: Add some content
  Given I have created a new film
  When I add file <file> as content
  Then the result should be only <display> in the content list

Examples:
	| file                      | display            |
	| /home/carl/DCP/bbc405.png | bbc405.png [still] |

