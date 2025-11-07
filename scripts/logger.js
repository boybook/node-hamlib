#!/usr/bin/env node

/**
 * Elegant logging utility for build processes
 * Provides tree-structured output with colors and progress indicators
 */

const chalk = require('chalk');
const ora = require('ora');

class Logger {
  constructor(title) {
    this.title = title;
    this.steps = [];
    this.currentStep = null;
    this.spinner = null;
    this.verbose = process.env.VERBOSE === 'true' || process.argv.includes('--verbose');
  }

  /**
   * Start a build process with a title
   */
  start() {
    console.log(chalk.bold.cyan(this.title));
  }

  /**
   * Add a new step to the build process
   * @param {string} name - Step name
   * @param {number} index - Step index
   * @param {number} total - Total steps
   */
  step(name, index, total) {
    if (this.currentStep) {
      this.endStep(true);
    }

    this.currentStep = {
      name,
      index,
      total,
      logs: []
    };

    const prefix = index < total ? '├─' : '└─';
    console.log(`${prefix} ${chalk.dim(`[${index}/${total}]`)} ${name}...`);
  }

  /**
   * Log a sub-item within current step
   * @param {string} message
   * @param {string} type - 'info', 'success', 'error', 'warning'
   */
  log(message, type = 'info') {
    if (!this.currentStep) return;

    const icons = {
      info: chalk.blue('⠋'),
      success: chalk.green('✓'),
      error: chalk.red('✗'),
      warning: chalk.yellow('⚠')
    };

    const colors = {
      info: chalk.blue,
      success: chalk.green,
      error: chalk.red,
      warning: chalk.yellow
    };

    const icon = icons[type] || icons.info;
    const color = colors[type] || chalk.white;

    console.log(`│  ${icon} ${color(message)}`);

    if (this.currentStep) {
      this.currentStep.logs.push({ message, type });
    }
  }

  /**
   * Start a spinner for long-running operation
   * @param {string} text
   */
  startSpinner(text) {
    if (this.spinner) {
      this.spinner.stop();
    }
    this.spinner = ora({
      text,
      prefixText: '│ ',
      color: 'cyan'
    }).start();
  }

  /**
   * Stop the spinner with success
   * @param {string} text
   */
  succeedSpinner(text) {
    if (this.spinner) {
      this.spinner.succeed(text);
      this.spinner = null;
    }
  }

  /**
   * Stop the spinner with failure
   * @param {string} text
   */
  failSpinner(text) {
    if (this.spinner) {
      this.spinner.fail(text);
      this.spinner = null;
    }
  }

  /**
   * End the current step
   * @param {boolean} success
   */
  endStep(success) {
    if (this.spinner) {
      this.spinner.stop();
      this.spinner = null;
    }

    if (this.currentStep) {
      this.currentStep = null;
    }
  }

  /**
   * Display verbose log
   * @param {string} message
   */
  debug(message) {
    if (this.verbose) {
      console.log(chalk.dim(`│  ${message}`));
    }
  }

  /**
   * Complete the build successfully
   * @param {number} duration - Duration in seconds
   */
  success(duration) {
    if (this.currentStep) {
      this.endStep(true);
    }

    const durationStr = duration ? chalk.dim(`(${duration.toFixed(1)}s)`) : '';
    console.log();
    console.log(chalk.green.bold(`✓ Build successful!`) + ` ${durationStr}`);
    console.log();
  }

  /**
   * Display an error and terminate
   * @param {string} title - Error title
   * @param {string} message - Error message
   * @param {string[]} solutions - Suggested solutions
   */
  error(title, message, solutions = []) {
    if (this.spinner) {
      this.spinner.stop();
      this.spinner = null;
    }

    if (this.currentStep) {
      console.log(`│  ${chalk.red('✗')} ${chalk.red('Failed')}`);
      console.log('│');
    } else {
      console.log();
    }

    console.log(`│  ${chalk.red.bold('Error:')} ${title}`);

    if (message) {
      console.log('│');
      message.split('\n').forEach(line => {
        console.log(`│  ${line}`);
      });
    }

    if (solutions.length > 0) {
      console.log('│');
      console.log(`│  ${chalk.cyan('To fix this issue:')}`);
      solutions.forEach(solution => {
        console.log(`│  ${chalk.cyan('•')} ${solution}`);
      });
    }

    console.log('│');
    console.log(`└─ ${chalk.red('Build failed')}`);
    console.log();
  }

  /**
   * Display a warning
   * @param {string} message
   */
  warning(message) {
    if (this.currentStep) {
      console.log(`│  ${chalk.yellow('⚠')} ${chalk.yellow(message)}`);
    } else {
      console.log();
      console.log(`${chalk.yellow('⚠')} ${chalk.yellow(message)}`);
      console.log();
    }
  }

  /**
   * Display general information
   * @param {string} message
   */
  info(message) {
    console.log();
    console.log(chalk.blue('ℹ') + ' ' + message);
    console.log();
  }

  /**
   * Display a section header
   * @param {string} title
   */
  section(title) {
    console.log();
    console.log(chalk.bold.underline(title));
    console.log();
  }

  /**
   * Display a table of data
   * @param {Array<{key: string, value: string}>} data
   */
  table(data) {
    const maxKeyLength = Math.max(...data.map(d => d.key.length));
    data.forEach(({ key, value }) => {
      const padding = ' '.repeat(maxKeyLength - key.length + 2);
      console.log(`  ${chalk.dim(key)}${padding}${value}`);
    });
    console.log();
  }
}

/**
 * Create a new logger instance
 * @param {string} title - Build process title
 * @returns {Logger}
 */
function createLogger(title) {
  return new Logger(title);
}

module.exports = {
  Logger,
  createLogger
};
