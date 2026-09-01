.. _sdhc:

SDHC
====

路径
---------------

.. code-block::

    zephyr/tests/drivers/sdhc
    zephyr/tests/drivers/disk/disk_access
    zephyr/tests/drivers/disk/disk_performance
    zephyr/tests/subsys/sd/sdmmc
    zephyr/tests/subsys/sd/mmc

命令行
-----------

As dust-hpm6750 for example:

.. code-block:: console

    west build -p always -b dust-hpm6750 -S sdhc zephyr/tests/drivers/sdhc