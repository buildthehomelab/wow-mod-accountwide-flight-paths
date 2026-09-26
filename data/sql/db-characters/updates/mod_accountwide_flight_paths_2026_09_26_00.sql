-- Every flight path (TaxiNodes.dbc id) known by a character on the account.
CREATE TABLE IF NOT EXISTS `accountwide_flight_paths` (
    `account_id`    INT UNSIGNED NOT NULL,
    `node`          INT UNSIGNED NOT NULL,
    PRIMARY KEY (`account_id`, `node`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
