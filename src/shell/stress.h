#ifndef STRESS_H
#define STRESS_H

/**
 * @file stress.h
 * @brief Header file for stress test commands, including hang, nohang, and recur.
 */

/**
 * @brief Hang command that spawns 10 children and block-waits for any of them until all are reaped.
 *
 * This command spawns 10 children and block-waits for any of them in a loop until all of them are reaped.
 * Note that the order in which the children get reaped is non-deterministic.
 */
void hang(void);

/**
 * @brief Nohang command that spawns 10 children and nonblocking waits for any of them until all are reaped.
 *
 * This command spawns 10 children and nonblocking waits for any of them in a loop until all of them are reaped.
 * Note that the order in which the children get reaped is non-deterministic.
 */
void nohang(void);

/**
 * @brief Recur command that recursively spawns itself 26 times, naming the spawned processes Gen_A through Gen_Z.
 *
 * Each process is block-waited and reaped by its parent.
 */
void recur(void);

#endif
