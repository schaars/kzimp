oset
=====

Set that remembers original insertion order.

Runs on Py2.5 or later (and runs on 3.0 or later without any modifications). For Python2.5, a local backport of ABC classes is also used.

Implementation based on a doubly linked link and an internal dictionary. This design gives OrderedSet the same big-Oh running times as regular sets including O(1) adds, removes, and lookups as well as O(n) iteration.

Usage
-----

Import and create ordered set.
::

    >>> from oset import oset
    >>> os = oset()

Requires
-------- 

- Python 2.5+

Changes
=======

Version 0.1
-------------
- http://code.activestate.com/recipes/576694-orderedset/
- Raymond Hettinger, 19 Mar 2009
 
Contributors
============
  
- Raymond Hettinger, (All kudos to him :)

- Carlos Martin <inean.es@gmail.com>

under the `Python Software Foundation License 
<http://www.opensource.org/licenses/PythonSoftFoundation.php>`_.
